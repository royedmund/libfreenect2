// Kinect v2 RGB camera to a V4L2 loopback device.
// Outputs RGB24 at 1280x720 (default) or 1920x1080 (KINECT_RESOLUTION=1080p).
// Uses only libfreenect2 + Linux V4L2; NO OBS, OpenCV, or Protonect window.
// Source corresponds to the hardware-tested x570-debian RGB24 implementation.
// Copyright (c) 2026 Roy Edmund Antaw and contributors.
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only
// This is independent supplementary code, not part of upstream OpenKinect.
#include <libfreenect2/libfreenect2.hpp>
#include <libfreenect2/frame_listener_impl.h>
#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <linux/videodev2.h>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>
#include <vector>

namespace {
volatile std::sig_atomic_t stopping = 0;
void on_signal(int) { stopping = 1; }

bool set_camera_format(int fd, int width, int height, v4l2_format &format) {
  std::memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  format.fmt.pix.width = static_cast<unsigned>(width);
  format.fmt.pix.height = static_cast<unsigned>(height);
  format.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;
  format.fmt.pix.field = V4L2_FIELD_NONE;
  format.fmt.pix.bytesperline = static_cast<unsigned>(width * 3);
  format.fmt.pix.sizeimage = static_cast<unsigned>(width * height * 3);
  format.fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;
  if (ioctl(fd, VIDIOC_S_FMT, &format) == -1) {
    std::cerr << "VIDIOC_S_FMT failed: " << std::strerror(errno) << '\n';
    return false;
  }
  if (format.fmt.pix.width != static_cast<unsigned>(width) ||
      format.fmt.pix.height != static_cast<unsigned>(height) ||
      format.fmt.pix.pixelformat != V4L2_PIX_FMT_RGB24 ||
      format.fmt.pix.bytesperline < static_cast<unsigned>(width * 3) ||
      format.fmt.pix.sizeimage < format.fmt.pix.bytesperline * static_cast<unsigned>(height)) {
    std::cerr << "The virtual webcam rejected the requested RGB24 resolution/stride.\n";
    return false;
  }
  // Nominal rate; the hardware produces the actual frames (usually 30 fps).
  v4l2_streamparm parm{};
  parm.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  parm.parm.output.timeperframe.numerator = 1;
  parm.parm.output.timeperframe.denominator = 30;
  (void)ioctl(fd, VIDIOC_S_PARM, &parm);
  return true;
}

bool convert_color(const libfreenect2::Frame &src, std::vector<unsigned char> &dst,
                   int width, int height, unsigned stride) {
  if (!src.data || src.width == 0 || src.height == 0 || src.bytes_per_pixel != 4 ||
      (src.format != libfreenect2::Frame::BGRX && src.format != libfreenect2::Frame::RGBX)) {
    std::cerr << "Unsupported Kinect colour frame format: " << static_cast<int>(src.format) << '\n';
    return false;
  }
  const bool bgrx = src.format == libfreenect2::Frame::BGRX;
  for (int y = 0; y < height; ++y) {
    const size_t sy = static_cast<size_t>(y) * src.height / static_cast<size_t>(height);
    const unsigned char *source_row = src.data + sy * src.width * 4;
    unsigned char *target_row = dst.data() + static_cast<size_t>(y) * stride;
    for (int x = 0; x < width; ++x) {
      const size_t sx = static_cast<size_t>(x) * src.width / static_cast<size_t>(width);
      const unsigned char *pixel = source_row + sx * 4;
      unsigned char *out = target_row + static_cast<size_t>(x) * 3;
      out[0] = bgrx ? pixel[0] : pixel[2];
      out[1] = pixel[1];
      out[2] = bgrx ? pixel[2] : pixel[0];
    }
  }
  return true;
}
} // namespace

int main(int argc, char **argv) {
  const std::string path = argc > 1 ? argv[1] : "/dev/video10";
  const char *mode = std::getenv("KINECT_RESOLUTION");
  const std::string resolution = mode ? mode : "720p";
  if (resolution != "720p" && resolution != "1080p") {
    std::cerr << "KINECT_RESOLUTION must be either 720p or 1080p.\n";
    return 2;
  }
  const int width = resolution == "1080p" ? 1920 : 1280;
  const int height = resolution == "1080p" ? 1080 : 720;
  std::signal(SIGINT, on_signal);
  std::signal(SIGTERM, on_signal);

  // Open the producer endpoint first. If OBS is using it, leave Kinect untouched.
  const int fd = open(path.c_str(), O_WRONLY | O_CLOEXEC);
  if (fd < 0) {
    std::cerr << "Cannot open " << path << " for output: " << std::strerror(errno)
              << ". Stop OBS Virtual Camera and check permissions.\n";
    return 1;
  }
  v4l2_format format{};
  if (!set_camera_format(fd, width, height, format)) {
    close(fd);
    return 1;
  }
  std::vector<unsigned char> image(format.fmt.pix.sizeimage, 0);
  std::cout << "Virtual camera: " << path << " (RGB24 " << width << 'x' << height << ")\n";

  libfreenect2::Freenect2 context;
  if (context.enumerateDevices() < 1) {
    std::cerr << "Kinect v2 not found. Check its power/USB 3.0 cable and udev permissions.\n";
    close(fd);
    return 1;
  }
  libfreenect2::Freenect2Device *device = context.openDevice(context.getDefaultDeviceSerialNumber());
  if (!device) {
    std::cerr << "Could not open Kinect. Close Protonect/other Kinect programs first.\n";
    close(fd);
    return 1;
  }
  libfreenect2::SyncMultiFrameListener listener(libfreenect2::Frame::Color);
  libfreenect2::FrameMap frames;
  device->setColorFrameListener(&listener);
  int result = 0;
  bool started = false;
  if (!(started = device->startStreams(true, false))) {
    std::cerr << "Kinect RGB stream failed to start.\n";
    result = 1;
  } else {
    std::cout << "Kinect RGB stream started (depth processing disabled).\n" << std::flush;
    unsigned timeouts = 0;
    while (!stopping) {
      // Short wait permits service termination without a forced kill.
      if (!listener.waitForNewFrame(frames, 1000)) {
        if (stopping) break;
        if (++timeouts >= 20) {
          std::cerr << "No colour frames received for 20 seconds. Restarting.\n";
          result = 1;
          break;
        }
        continue;
      }
      timeouts = 0;
      auto it = frames.find(libfreenect2::Frame::Color);
      if (it == frames.end() || !it->second ||
          !convert_color(*it->second, image, width, height, format.fmt.pix.bytesperline)) {
        std::cerr << "Bad or unsupported Kinect colour frame.\n";
        listener.release(frames);
        result = 1;
        break;
      }
      listener.release(frames);
      ssize_t count;
      do { count = write(fd, image.data(), image.size()); }
      while (count == -1 && errno == EINTR && !stopping);
      if (stopping) break;
      if (count != static_cast<ssize_t>(image.size())) {
        std::cerr << "Virtual camera write failed or incomplete: " << std::strerror(errno) << '\n';
        result = 1;
        break;
      }
    }
  }
  if (started) device->stop();
  device->close();
  close(fd);
  return result;
}

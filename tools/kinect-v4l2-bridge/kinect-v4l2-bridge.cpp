// Experimental Kinect v2 RGB camera -> V4L2 loopback webcam.
// Independent community addition; not an upstream OpenKinect driver.
// Copyright (c) 2026 Roy Edmund Antaw and contributors.
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <libfreenect2/libfreenect2.hpp>
#include <libfreenect2/frame_listener_impl.h>
#include <linux/videodev2.h>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>
#include <vector>

namespace {
volatile sig_atomic_t running = 1;
void stop_signal(int) { running = 0; }

int positive(const std::string &s) {
  try {
    std::size_t end = 0;
    int value = std::stoi(s, &end);
    if (value > 0 && end == s.size()) return value;
  } catch (...) {}
  throw std::runtime_error("Invalid positive integer: " + s);
}
uint8_t clamp(int n) {
  return static_cast<uint8_t>(n < 0 ? 0 : (n > 255 ? 255 : n));
}
struct Yuv { int y, u, v; };
Yuv sample(const libfreenect2::Frame &f, int x, int y) {
  const uint8_t *p = f.data +
      (static_cast<std::size_t>(y) * f.width + x) * f.bytes_per_pixel;
  const int r = f.format == libfreenect2::Frame::RGBX ? p[0] : p[2];
  const int g = p[1];
  const int b = f.format == libfreenect2::Frame::RGBX ? p[2] : p[0];
  // BT.601 (limited range) approximation.
  return {(66*r + 129*g + 25*b + 128)/256 + 16,
          (-38*r - 74*g + 112*b + 128)/256 + 128,
          (112*r - 94*g - 18*b + 128)/256 + 128};
}
void to_yuyv(const libfreenect2::Frame &src, int w, int h,
             std::vector<uint8_t> &dst) {
  // Basic nearest-neighbour resize + packed YUYV422.
  for (int y = 0; y < h; ++y) {
    int sy = static_cast<int>(static_cast<std::size_t>(y) * src.height / h);
    for (int x = 0; x < w; x += 2) {
      int sx0 = static_cast<int>(static_cast<std::size_t>(x) * src.width / w);
      int sx1 = static_cast<int>(static_cast<std::size_t>(x+1) * src.width / w);
      Yuv a = sample(src, sx0, sy), b = sample(src, sx1, sy);
      std::size_t offset = (static_cast<std::size_t>(y) * w + x) * 2;
      dst[offset]     = clamp(a.y);
      dst[offset + 1] = clamp((a.u + b.u) / 2);
      dst[offset + 2] = clamp(b.y);
      dst[offset + 3] = clamp((a.v + b.v) / 2);
    }
  }
}
void output_frame(int fd, const std::vector<uint8_t> &frame) {
  while (running) {
    ssize_t n = ::write(fd, frame.data(), frame.size());
    if (n == static_cast<ssize_t>(frame.size())) return;
    if (n < 0 && errno == EINTR) continue;
    if (n < 0) throw std::runtime_error(std::string("V4L2 write: ") + std::strerror(errno));
    throw std::runtime_error("Unexpected partial/empty V4L2 frame write");
  }
}
struct KinectHandle {
  libfreenect2::Freenect2Device *device = nullptr;
  bool streaming = false;
  ~KinectHandle() {
    if (device) {
      if (streaming) device->stop();
      device->close();
    }
  }
};
struct Fd {
  int fd;
  ~Fd() { if (fd >= 0) ::close(fd); }
};
} // namespace

int main(int argc, char **argv) {
  std::string node = "/dev/video10";
  int width = 1280, height = 720;
  try {
    for (int i = 1; i < argc; ++i) {
      std::string flag = argv[i];
      if (flag == "--help" || flag == "-h") {
        std::cout << "Usage: kinect-v4l2-bridge [--device /dev/video10] "
                     "[--width 1280] [--height 720]\n";
        return 0;
      }
      if (i + 1 >= argc) throw std::runtime_error("Missing value for " + flag);
      std::string val = argv[++i];
      if (flag == "--device") node = val;
      else if (flag == "--width") width = positive(val);
      else if (flag == "--height") height = positive(val);
      else throw std::runtime_error("Unknown flag " + flag);
    }
    if (width % 2 || width > 1920 || height > 1080 ||
        static_cast<long long>(width) * height > 1920LL * 1080LL)
      throw std::runtime_error("Width must be even; maximum size is 1920x1080");

    std::signal(SIGINT, stop_signal);
    std::signal(SIGTERM, stop_signal);

    libfreenect2::Freenect2 ctx;
    if (ctx.enumerateDevices() == 0)
      throw std::runtime_error("No Kinect v2 detected: check power, USB3 and udev");
    KinectHandle camera;
    camera.device = ctx.openDefaultDevice();
    if (!camera.device) throw std::runtime_error("Cannot open Kinect (already in use?)");
    libfreenect2::SyncMultiFrameListener listener(libfreenect2::Frame::Color);
    camera.device->setColorFrameListener(&listener);

    int rawfd = ::open(node.c_str(), O_WRONLY | O_CLOEXEC);
    if (rawfd < 0) throw std::runtime_error("Cannot open " + node + ": " + std::strerror(errno));
    Fd video{rawfd};

    v4l2_capability caps{};
    if (::ioctl(rawfd, VIDIOC_QUERYCAP, &caps) < 0)
      throw std::runtime_error("VIDIOC_QUERYCAP failed");
    uint32_t flags = (caps.capabilities & V4L2_CAP_DEVICE_CAPS) ? caps.device_caps : caps.capabilities;
    if (!(flags & V4L2_CAP_VIDEO_OUTPUT))
      throw std::runtime_error("Device does not accept V4L2 video output");

    v4l2_format fmt{};
    fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    fmt.fmt.pix.width = static_cast<uint32_t>(width);
    fmt.fmt.pix.height = static_cast<uint32_t>(height);
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    fmt.fmt.pix.bytesperline = static_cast<uint32_t>(width * 2);
    fmt.fmt.pix.sizeimage = static_cast<uint32_t>(width * height * 2);
    if (::ioctl(rawfd, VIDIOC_S_FMT, &fmt) < 0)
      throw std::runtime_error(std::string("VIDIOC_S_FMT: ") + std::strerror(errno));
    if (fmt.fmt.pix.width != static_cast<uint32_t>(width) ||
        fmt.fmt.pix.height != static_cast<uint32_t>(height) ||
        fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV ||
        fmt.fmt.pix.bytesperline != static_cast<uint32_t>(width * 2))
      throw std::runtime_error("V4L2 device refused requested YUYV size/stride");

    if (!camera.device->startStreams(true, false))
      throw std::runtime_error("Kinect colour streaming failed");
    camera.streaming = true;
    std::vector<uint8_t> pixels(static_cast<std::size_t>(width) * height * 2);
    std::cerr << "Kinect v2 -> " << node << " (" << width << 'x' << height
              << " YUYV). Press Ctrl+C to stop.\n";
    std::size_t frame_count = 0;
    while (running) {
      libfreenect2::FrameMap frames;
      if (!listener.waitForNewFrame(frames, 2000)) {
        std::cerr << "Timed out waiting for colour frame\n";
        continue;
      }
      auto color = frames.find(libfreenect2::Frame::Color);
      if (color != frames.end() && color->second) {
        const libfreenect2::Frame &src = *color->second;
        if (src.data && src.status == 0 && src.bytes_per_pixel == 4 &&
            (src.format == libfreenect2::Frame::RGBX ||
             src.format == libfreenect2::Frame::BGRX)) {
          to_yuyv(src, width, height, pixels);
          listener.release(frames);
          output_frame(rawfd, pixels);
          if (++frame_count % 300 == 0) std::cerr << "Frames sent: " << frame_count << '\n';
        } else {
          std::cerr << "Skipping unsupported/error colour frame\n";
          listener.release(frames);
        }
      } else listener.release(frames);
    }
    return 0;
  } catch (const std::exception &ex) {
    std::cerr << "kinect-v4l2-bridge: " << ex.what() << '\n';
    return 1;
  }
}

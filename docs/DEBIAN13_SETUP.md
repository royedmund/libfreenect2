# Build and test Kinect v2 / libfreenect2 on Debian 13

This guide applies to **Kinect for Windows v2 / Kinect for Xbox One** sensors and a Debian 13 x86_64 desktop. Kinect v1 is **not** supported by libfreenect2. A working USB 3.x host connection and a graphical desktop are required for the standard preview.

These commands describe a reproducible installation path; this document does not claim that every GPU/driver combination has been tested. The upstream project remains the authoritative source for the full hardware compatibility matrix.

## 1. Prerequisites

Open a Debian terminal as your normal user. Install build dependencies and test utilities:

```bash
sudo apt update
sudo apt install -y \
  build-essential cmake pkg-config git \
  libusb-1.0-0-dev libturbojpeg0-dev \
  libglfw3-dev libgl-dev \
  ocl-icd-opencl-dev opencl-headers \
  usbutils clinfo
```

OpenCL depth processing also needs a **working vendor OpenCL ICD/driver**, which is not automatically provided by the headers and ICD loader. Check `clinfo` and your GPU vendor's Debian-compatible runtime. If unavailable, you can still test OpenGL (`gl`) or CPU (`cpu`) paths.

For a minimal CPU/OpenGL build, omit the OpenCL packages and set `-DENABLE_OPENCL=OFF` in the configuration command. Optional CUDA, OpenNI2, VAAPI and other upstream components are **not** required for the basic preview.

## 2. Clone the GitHub fork

After the maintainer has published the fork:

```bash
git clone https://github.com/royedmund/libfreenect2.git
cd libfreenect2
```

For local development before publication, run the following steps from the unpacked project directory instead.

## 3. Configure and build

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_OPENCL=ON \
  -DENABLE_OPENGL=ON \
  -DENABLE_CUDA=OFF

cmake --build build --parallel 4
```

Choose `-DENABLE_OPENCL=OFF` if you intentionally want only OpenGL/CPU operation. Examine CMake's final feature summary and confirm that the selected pipeline was compiled. CMake cache files from another computer must **not** be reused; build in a fresh `build/` directory.

## 4. USB access (avoid running Protonect as root)

Plug the Kinect into a proper USB 3.x port with its required power supply. Check:

```bash
lsusb
lsusb -t
```

Upstream supplies `platform/linux/udev/90-kinect2.rules`, whose `MODE="0666"` grants access to all local users. On a shared machine, the following alternative is more restrictive. Use **one approach**, not conflicting copies of both rules:

```bash
sudo usermod -aG video "$USER"
sudo tee /etc/udev/rules.d/90-kinect2-local.rules >/dev/null <<'RULES'
SUBSYSTEM=="usb", ATTR{idVendor}=="045e", ATTR{idProduct}=="02c4", GROUP="video", MODE="0660", TAG+="uaccess"
SUBSYSTEM=="usb", ATTR{idVendor}=="045e", ATTR{idProduct}=="02d8", GROUP="video", MODE="0660", TAG+="uaccess"
SUBSYSTEM=="usb", ATTR{idVendor}=="045e", ATTR{idProduct}=="02d9", GROUP="video", MODE="0660", TAG+="uaccess"
RULES
sudo udevadm control --reload-rules
sudo udevadm trigger --subsystem-match=usb
```

Then unplug/replug the Kinect and **log out and back in** so the new `video` group membership applies. Existing successful access may not require any rule changes. Confirm with `id -nG` and `ls -l /dev/bus/usb/...` for the actual Kinect node.

## 5. Test the camera

Use the graphics pipeline that is available on your computer:

```bash
./build/bin/Protonect gl
```

If OpenCL works and was enabled:

```bash
./build/bin/Protonect cl
```

For a CPU-only diagnostic:

```bash
./build/bin/Protonect cpu
```

**Expected result:** the program detects the Kinect and shows a preview with RGB/depth data. Keep the window open if you plan to capture it in OBS. If you see `LIBUSB_ERROR_ACCESS`, check the udev rule, group membership and device permissions. If the application says no Kinect detected, check the USB 3 connection and power first.

The source archive's `build/` files were generated on the maintainer's computer and should **not** be copied into a published repository or relied upon by new users.

## 6. Diagnostic helper

```bash
./scripts/check-kinect-debian13.sh
```

This helper reports USB devices, video devices, relevant permissions and optional tooling. It does not change your system.

## 7. What this library does — and does not — do

libfreenect2 exposes Kinect RGB, infrared and depth streams to applications. It **does not automatically create a webcam device** such as `/dev/video10` for Microsoft Teams. For practical Teams use, follow [`TEAMS_FOR_LINUX.md`](TEAMS_FOR_LINUX.md): capture the live Protonect preview in OBS Studio and expose it using OBS Virtual Camera.

Links: [OpenKinect upstream](https://github.com/OpenKinect/libfreenect2) · [libfreenect2 troubleshooting](https://github.com/OpenKinect/libfreenect2/wiki/Troubleshooting)

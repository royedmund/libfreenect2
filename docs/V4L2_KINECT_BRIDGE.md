# Kinect v2 RGB24 webcam on Debian 13 — reproduced X570 configuration

This guide explains how to make a **Microsoft Kinect for Windows v2 / Xbox One Kinect** appear as a Linux webcam, without an OBS window or Protonect preview, then use it in the community-maintained **Teams-for-Linux** client.

**Project:** [Roy's libfreenect2 fork](https://github.com/royedmund/libfreenect2) of [OpenKinect/libfreenect2](https://github.com/OpenKinect/libfreenect2).

**Verification status (7 October 2026):** The **RGB24 bridge implementation** supplied in this fork was observed running on an X570 Debian 13.7 desktop with an AMD Ryzen 7 5800X and an NVIDIA RTX 4070. The Kinect enumerated on a 5 Gbit/s USB 3 connection; `/dev/video10` was available and delivered frames; Teams-for-Linux displayed a working live camera image without OBS or Protonect. The running service, executable, source and library location were inspected. **The new clean-install script and the new `~/.local` build/link method have not yet been tested on a second, clean machine.** They are provided for that independent reproducibility test, not claimed as proven. The previously published **YUYV experimental example** has been replaced here by the actual **RGB24** source from the working X570.

## 1. Architecture

A powered Kinect v2 sends its colour stream over USB 3 to libfreenect2. The separate `kinect-v2-bridge` converts those frames to RGB24 and writes to `v4l2loopback` at `/dev/video10`, which Teams-for-Linux and other V4L2 clients can open.

The bridge provides **video only**. It does not supply microphone audio, skeleton tracking, depth or infrared streams to Teams. Webcam frame output is set to 1280×720 by default; setting `KINECT_RESOLUTION=1080p` selects 1920×1080. The original source's packed-colour conversion is retained unchanged because this is the version tested on X570. Test red/blue objects on any new platform before asserting channel correctness.

## 2. Requirements for a fresh Debian 13 desktop

You need a powered Kinect v2, a USB 3-capable host port, a Debian 13 desktop user with `sudo`, and internet access to install Debian packages and clone GitHub. The instructions expect a systemd-based Debian installation, a **local graphical login** (for Teams), and a kernel whose headers are available.

**Do not run the installation script with `sudo`**; it uses `sudo` internally for only the operations requiring administrative privileges. On the first setup, make sure Protonect and OBS Virtual Camera are stopped. The installer intentionally refuses to replace a pre-existing bridge/service unless you explicitly pass `--replace-existing`.

Clone the fork into a separate directory (don't overwrite an existing OpenKinect checkout):

```bash
sudo apt update
sudo apt install -y git

git clone https://github.com/royedmund/libfreenect2.git ~/libfreenect2-royedmund
cd ~/libfreenect2-royedmund
```

**Recommended first test** — install and configure the camera, but defer starting the background service until the system is ready:

```bash
./scripts/install-kinect-v2-webcam.sh --no-start
```

The installer performs these operations, in order:

1. Installs the C++/CMake build dependencies, libusb, TurboJPEG, GLFW, `v4l2loopback-dkms`, `v4l-utils`, and matching kernel headers if needed.
2. Creates a restricted Kinect USB udev rule using the Linux `video` group (unless you already have the local rule), and adds the current user to that group when necessary.
3. Configures a `v4l2loopback` camera with the label **Microsoft Kinect v2** at **`/dev/video10`**, and sets module loading at boot. It will not unload an in-use module. If another camera uses device 10, address that collision before continuing.
4. Compiles the fork's core libfreenect2 into **`~/.local`** and compiles the separate RGB24 bridge against that installed copy.
5. Installs `~/.local/bin/kinect-v2-bridge` and the portable `~/.config/systemd/user/kinect-v2-webcam.service` unit. The unit uses `%h` instead of a hardcoded `/home/royed` path.

**Important:** The older working X570 executable linked to `/home/royed/libfreenect2/build/lib/libfreenect2.so.0.2` using its RUNPATH. The installer deliberately replaces that *new-machine* dependency pattern with a per-user `~/.local/lib` installation. This is a different deployment route and needs a clean-machine acceptance test.

### Verify permissions, kernel module and binary

After installation, **log out and in** if the installer has added you to the `video` group. If the module was already loaded with different options, you may need a reboot to apply the new module options.

```bash
id -nG                         # should include video
lsusb -t                        # Kinect sensor should be on 5000M USB3
v4l2-ctl --list-devices        # Microsoft Kinect v2, /dev/video10
ls -l /dev/video10
ldd ~/.local/bin/kinect-v2-bridge | grep -Ei 'freenect|usb|jpeg'
readelf -d ~/.local/bin/kinect-v2-bridge | grep -E 'RPATH|RUNPATH'
```

The libfreenect2 dependency should resolve into **your own `~/.local/lib`** rather than the maintainer's old build directory. If it does not, do not mark installation successful; follow the troubleshooting section.

### Start the service

If everything looks correct, enable it:

```bash
systemctl --user daemon-reload
systemctl --user enable --now kinect-v2-webcam.service
systemctl --user status kinect-v2-webcam.service --no-pager -l
journalctl --user -u kinect-v2-webcam.service -n 30 --no-pager
```

The normal startup flow uses the `systemd` user session, so the virtual webcam appears when you log in. The service automatically retries after failures (15-second delay) and restarts if the sensor temporarily disappears. A user service is not a system-wide boot daemon; it does not automatically keep running when the user is fully logged out unless you separately configure user lingering (not needed here).

Alternatively, after the computer has a fresh user session, run without `--no-start` to install and enable the service in one pass:

```bash
./scripts/install-kinect-v2-webcam.sh
```

On an already-installed computer this may require `--replace-existing`, which should **only** be used after you have inspected and backed up the current bridge. Do not use it casually on your working X570 setup.

## 3. Test the V4L2 video output

When the service is running, check the camera and read frames:

```bash
v4l2-ctl -d /dev/video10 --all
v4l2-ctl -d /dev/video10 --stream-mmap --stream-count=90 --stream-poll
```

The X570 delivered a usable stream at about **15 fps** in the `v4l2-ctl` test, even though the driver had a **nominal 30 fps** setting. These are different measurements; do not claim 30 fps actually measured. It printed a benign-in-this-test message:

```text
VIDIOC_CREATE_BUFS returned -1 (Inappropriate ioctl for device)
```

This did **not** stop video capture on the tested machine. The reason was not investigated; the warning is not a universal guarantee of harmlessness.

For a graphical preview outside Teams:

```bash
sudo apt install -y ffmpeg
ffplay -f v4l2 -video_size 1280x720 -pixel_format rgb24 -i /dev/video10
```

The **full command** is needed; typing `ffplay` without `-i /dev/video10` only prints the program's usage message. The preview must be run on a graphical desktop, not an ordinary text-only SSH terminal. Close the viewer after your test.

## 4. Install and configure Teams-for-Linux

[Teams-for-Linux](https://github.com/IsmaelMartinez/teams-for-linux) is an **unofficial** Electron desktop wrapper around the Microsoft Teams web client, not a Microsoft-supported Linux app.

Follow the Teams-for-Linux project's [current install options](https://github.com/IsmaelMartinez/teams-for-linux#installation). For example, download the correct architecture `.deb` from its [GitHub Releases](https://github.com/IsmaelMartinez/teams-for-linux/releases) and install it from the directory you downloaded it to:

```bash
sudo apt install ./teams-for-linux_*.deb
```

Choose the downloaded filename appropriate for your release. Once installed:

```bash
teams-for-linux
```

In the Teams app, sign in, open **Settings → Devices** or the pre-call camera selector, and select **Microsoft Kinect v2**. The bridge must start **before Teams**; quit and restart Teams if you plug the Kinect in or start the bridge afterward. Select your **normal microphone and speaker** because the bridge exports only video. Check the live preview first, then test a call. On X570, an actual Teams-for-Linux session displayed a live RGB image from this exact bridge implementation with no OBS Studio or Protonect window.

**Suggested acceptance record for the second machine:** Debian version/kernel, CPU/GPU, USB link speed, `lsusb` Kinect VID/PID, installed dependency versions, actual bridge RUNPATH, `v4l2-ctl --all`, measured streaming fps, camera preview result, Teams app packaging/version, and whether any commands had to be changed.

## 5. Troubleshooting

| Symptom | What to check |
| --- | --- |
| `LIBUSB_ERROR_ACCESS` or no Kinect | Sensor power, USB 3 port, `lsusb -t`, udev rule, replug Kinect, log out/in after joining `video`. |
| `v4l2loopback` fails to load | Kernel headers must match `uname -r`; check `dkms status`, `journalctl -k`, Secure Boot/MOK module signing. Do not disable Secure Boot blindly. |
| `/dev/video10` doesn't exist | `lsmod | grep v4l2loopback`, module settings; reboot if module was already loaded with other options. |
| `/dev/video10` is already busy | Stop OBS Virtual Camera or an old `kinect-v2-webcam.service`; do not start two writers. |
| `pkg-config: Package freenect2 not found` | For manual builds: `export PKG_CONFIG_PATH="$HOME/.local/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"`. The installer sets this automatically. |
| `libfreenect2.so.0.2 => not found` | `ldd ~/.local/bin/kinect-v2-bridge`, inspect `readelf -d ...` for RUNPATH; verify installation in `~/.local/lib`. |
| Bridge starts but no video | Check `systemctl --user status`, `journalctl --user`, USB 3 link, and whether Protonect is already using the Kinect. |
| Teams doesn't list Kinect | Start bridge first, restart Teams, check Teams app sandbox webcam permission and `exclusive_caps=1`. |
| Low fps (~15 instead of nominal 30) | Document actual measured rate, check USB and CPU; do not confuse decoding throughput (~110 Hz in TurboJPEG logs) with camera fps. |
| Image colour looks wrong | Test red/blue objects; source colour-channel handling has not been separately validated by colour-chart measurement. |
| `VIDIOC_CREATE_BUFS returned -1` | If video still streams, document this as the observed v4l2loopback warning; diagnose further if frames do not follow. |

### Manual recovery / stop

```bash
systemctl --user stop kinect-v2-webcam.service
systemctl --user disable kinect-v2-webcam.service
```

You can run Protonect or OBS after stopping the bridge, but don't have multiple Kinect/virtual-camera producers using the same hardware concurrently.

## 6. Remove the optional webcam integration

If you no longer need the installed webcam service, remove only the files created for it:

```bash
systemctl --user disable --now kinect-v2-webcam.service
rm -f ~/.config/systemd/user/kinect-v2-webcam.service
rm -f ~/.local/bin/kinect-v2-bridge
systemctl --user daemon-reload
```

The **root-installed** file(s) `/etc/modprobe.d/kinect-v2-webcam.conf`, `/etc/modules-load.d/kinect-v2-webcam.conf`, and `/etc/udev/rules.d/90-kinect2-local.rules` should only be deleted after inspecting them and confirming no other application relies on them. Do not remove the shared `v4l2loopback` package or the `video` group membership automatically. The installer also installs libfreenect2 headers/libraries to `~/.local` which may be used by other software; leave those in place unless you understand their dependencies.

## 7. Source files and provenance

- **Bridge C++:** [`tools/kinect-v4l2-bridge/kinect-v4l2-bridge.cpp`](../tools/kinect-v4l2-bridge/kinect-v4l2-bridge.cpp)
- **Bridge CMake:** [`tools/kinect-v4l2-bridge/CMakeLists.txt`](../tools/kinect-v4l2-bridge/CMakeLists.txt)
- **Debian installation:** [`scripts/install-kinect-v2-webcam.sh`](../scripts/install-kinect-v2-webcam.sh)
- **User unit:** [`systemd/user/kinect-v2-webcam.service`](../systemd/user/kinect-v2-webcam.service)
- **Other Debian setup:** [`DEBIAN13_SETUP.md`](DEBIAN13_SETUP.md)
- **OBS fallback:** [`TEAMS_FOR_LINUX.md`](TEAMS_FOR_LINUX.md)

The C++ implementation in this update was taken from the **running X570 bridge source shared by the maintainer**, not invented to match its output. Only comments were added; its executable behaviour is intended to match the tested original. The new packaging, runtime linking approach and clean-install automation are newly documented/reworked and **not yet independently verified**. Preserve upstream OpenKinect copyright and licence notices when distributing the original libfreenect2 code. No webcam implementation in this fork should be mistaken for an official Microsoft driver.

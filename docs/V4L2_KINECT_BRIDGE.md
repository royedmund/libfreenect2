# Experimental Kinect v2 → V4L2 webcam bridge on Debian 13

This fork includes an **experimental, optional colour-only userspace bridge** that exposes Kinect v2 as a webcam by sending libfreenect2 RGB frames into `v4l2loopback` (typically `/dev/video10`). This avoids the Protonect preview window and OBS Studio.

**Status:** Source was checked with `g++ -std=c++11 -fsyntax-only` against the headers from the supplied project; it has **not been run with physical Kinect hardware or Microsoft Teams** here. Some v4l2loopback/Electron versions may need additional tuning. The working [Protonect → OBS procedure](TEAMS_FOR_LINUX.md) remains the fallback.

```text
Kinect for Windows v2 (power + USB 3)
   → libfreenect2 RGB frames
   → tools/kinect-v4l2-bridge (CPU resize and YUYV422 conversion)
   → v4l2loopback (/dev/video10)
   → Teams-for-Linux / Chromium / V4L2 webcam clients
```

This bridge is **video-only**: it does not provide audio, Kinect depth, infrared, camera controls, or an actual kernel/UVC webcam driver.

## Step 1 — Verify your camera and Debian installation

Follow [DEBIAN13_SETUP.md](DEBIAN13_SETUP.md) to build libfreenect2, apply USB udev permissions and verify `./build/bin/Protonect gl` works. **Quit Protonect before running the bridge** because both programs attempt to open the Kinect device.

## Step 2 — Install kernel loopback prerequisites

```bash
sudo apt update
sudo apt install -y build-essential cmake pkg-config \
    v4l2loopback-dkms v4l2loopback-utils v4l-utils
sudo apt install "linux-headers-$(uname -r)"
```

`v4l2loopback-dkms` must compile against matching kernel headers. If Debian doesn't offer headers for a custom kernel, use the headers supplied with that kernel. Secure Boot might require signing/enrolling the DKMS module; don't disable Secure Boot merely to bypass this issue.

First check whether another application already has a virtual camera:

```bash
v4l2-ctl --list-devices
lsmod | grep v4l2loopback || true
ls -l /dev/video10 2>/dev/null || true
```

When no conflicting loopback device is loaded and device 10 is available, create it:

```bash
sudo modprobe v4l2loopback \
    devices=1 video_nr=10 \
    card_label='Kinect v2 Virtual Camera' exclusive_caps=1
v4l2-ctl --list-devices
```

`exclusive_caps=1` helps camera enumeration by Chromium/Electron. With this flag, video capture may only become visible **after the bridge starts writing**. Do not unload `v4l2loopback` while OBS or other clients are using it.

If the manual test succeeds, persist the **module configuration**:

```bash
printf '%s\n' 'options v4l2loopback devices=1 video_nr=10 card_label="Kinect v2 Virtual Camera" exclusive_caps=1' | \
    sudo tee /etc/modprobe.d/kinect-v4l2.conf
printf '%s\n' v4l2loopback | sudo tee /etc/modules-load.d/kinect-v4l2.conf
```

If device 10 is taken, select a different free number in both the `modprobe` settings and bridge command. These files only ensure the *virtual device* is created at boot, not that the camera bridge starts automatically.

## Step 3 — Check permissions

```bash
ls -l /dev/video10
id -nG
```

Depending on system device ownership, you might need:

```bash
sudo usermod -aG video "$USER"
```

**Log out and back in** after modifying group membership. Also check the separate Kinect USB access rules in [DEBIAN13_SETUP.md](DEBIAN13_SETUP.md). Avoid running the bridge with `sudo`: fix device access instead.

## Step 4 — Install libfreenect2 and build the bridge

From the root of your checked-out fork (adjust the path if different):

```bash
cd ~/libfreenect2
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_OPENGL=ON -DENABLE_CUDA=OFF
cmake --build build --parallel 4
sudo cmake --install build
sudo ldconfig
pkg-config --modversion freenect2
```

If `pkg-config` cannot find `freenect2`, inspect the library installation prefix; for common `/usr/local` layouts:

```bash
export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
pkg-config --modversion freenect2
```

Build the standalone example:

```bash
cmake -S tools/kinect-v4l2-bridge \
    -B build/kinect-v4l2-bridge \
    -DCMAKE_BUILD_TYPE=Release
cmake --build build/kinect-v4l2-bridge --parallel 4
```

## Step 5 — Run and verify `/dev/video10`

Start the bridge **as your normal user**:

```bash
./build/kinect-v4l2-bridge/kinect-v4l2-bridge \
    --device /dev/video10 --width 1280 --height 720
```

It should log that video is streaming and periodically print a frame count. Keep it running. In a second terminal:

```bash
v4l2-ctl --list-devices
v4l2-ctl -d /dev/video10 --all
```

Optional independent video test (in a graphical session):

```bash
sudo apt install -y ffmpeg
ffplay -f v4l2 -input_format yuyv422 -video_size 1280x720 -i /dev/video10
```

Close the preview before Teams if your build only supports one consumer. For slower CPUs, restart at `--width 640 --height 360`. The current example converts frames on the CPU; it does not use OpenCL, CUDA or a GPU scaler. Press Ctrl+C to stop.

## Step 6 — Teams-for-Linux

Install Teams-for-Linux following [TEAMS_FOR_LINUX.md](TEAMS_FOR_LINUX.md). Teams-for-Linux is a community Electron wrapper around the Microsoft Teams web app, not an official Microsoft client.

1. Stop Protonect and OBS Virtual Camera. They must not compete with the bridge for the Kinect or loopback writer.
2. Start the bridge, and confirm that it is outputting frames.
3. Launch or fully restart Teams-for-Linux; select **Kinect v2 Virtual Camera** under **Settings → Devices** or the meeting-join preview.
4. Pick your normal microphone and speakers: this code provides **video only**.
5. If the camera does not appear, check permissions, Flatpak/Snap sandbox policies, Chromium/Electron V4L2 support and `exclusive_caps=1` behaviour. Fall back to the documented OBS method.

## Step 7 — Optional user systemd service (only after successful manual testing)

This starts the bridge automatically **when `royed`'s user session starts**. Only configure it after manually confirming the device works, and verify the actual path to the compiled binary:

```bash
mkdir -p ~/.config/systemd/user
cat > ~/.config/systemd/user/kinect-v4l2-bridge.service <<'UNIT'
[Unit]
Description=Experimental Kinect v2 webcam bridge
After=default.target

[Service]
Type=simple
ExecStart=%h/libfreenect2/build/kinect-v4l2-bridge/kinect-v4l2-bridge --device /dev/video10
Restart=on-failure
RestartSec=5

[Install]
WantedBy=default.target
UNIT
systemctl --user daemon-reload
systemctl --user enable --now kinect-v4l2-bridge.service
```

If your clone is somewhere else, **change `ExecStart` before enabling**. Stop or disable this service before you want to run Protonect or OBS:

```bash
systemctl --user disable --now kinect-v4l2-bridge.service
```

### Troubleshooting

| Problem | Investigate |
| --- | --- |
| `LIBUSB_ERROR_ACCESS` | Kinect udev rule, group membership, device reconnect, USB 3. |
| No Kinect device | Power supply, correct Kinect v2 sensor, USB 3 port, `lsusb -t`. |
| Kinect busy | Quit Protonect, other bridge processes and camera applications. |
| No `/dev/video10` | `dkms status`, headers, module load, Secure Boot, device number conflicts. |
| `/dev/video10` permission denied | Group membership and `/dev/video10` permissions. |
| `VIDIOC_S_FMT` failure | Device must be a V4L2 **output** node from v4l2loopback; stop OBS. |
| Bridge build cannot locate `freenect2` | `sudo cmake --install build`, `pkg-config`, and library prefix. |
| Camera not listed in Teams | Start bridge before Teams; check `exclusive_caps`, sandbox and app restart. |
| High CPU load / stutter | Try 640×360; inspect USB throughput and host load. |

## Project attribution

Original libfreenect2 remains credited and licensed by the OpenKinect contributors. The new standalone bridge is independently contributed to this fork and is dual-licensed **Apache-2.0 OR GPL-2.0-only**. It is not an official OpenKinect, Microsoft or Teams driver.

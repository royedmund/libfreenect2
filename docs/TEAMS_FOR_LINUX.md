# Kinect v2 as a Teams-for-Linux camera on Debian 13

**Documented path (not a native libfreenect2 webcam driver):**

`Kinect v2 → libfreenect2/Protonect preview → OBS Studio → OBS Virtual Camera (/dev/video*) → Teams-for-Linux`

The uploaded fork code contains no native Kinect-to-V4L2 bridge. OBS captures the running preview window; it does **not** consume RGB frames directly from libfreenect2. This solution is useful for experimentation but is less seamless than a real webcam driver.

Teams-for-Linux is a community-maintained wrapper around the Teams web application, **not an official Microsoft Linux client**. Some capabilities depend on Microsoft's current web application and the Electron version. See [Teams-for-Linux](https://github.com/IsmaelMartinez/teams-for-linux).

## 1. Build and verify Kinect v2

Follow [`DEBIAN13_SETUP.md`](DEBIAN13_SETUP.md), then run:

```bash
cd ~/libfreenect2
./build/bin/Protonect gl
```

If your checkout is located elsewhere, substitute that directory. Confirm that a usable live Kinect preview appears **before** launching OBS. Keep the `Protonect` window running while you use Teams.

## 2. Install OBS Studio and its Linux virtual-camera dependency

```bash
sudo apt update
sudo apt install -y obs-studio v4l2loopback-dkms v4l2loopback-utils v4l-utils
```

`v4l2loopback-dkms` needs kernel headers that match the **currently running** Debian kernel. If installation reports missing headers, use the Debian kernel header package matching `uname -r`, then reinstall/rebuild the DKMS module. If Secure Boot is enabled, an unsigned DKMS module may be blocked until it is signed/enrolled. Do not disable Secure Boot blindly.

Create a dedicated virtual webcam endpoint for OBS (while no other virtual camera is active):

```bash
sudo modprobe v4l2loopback \
  devices=1 video_nr=10 \
  card_label='Kinect v2 (OBS Virtual Camera)' \
  exclusive_caps=1

v4l2-ctl --list-devices
ls -l /dev/video10
```

The label and `/dev/video10` are examples; another video number may be allocated if a driver already owns it. `exclusive_caps=1` is commonly needed for browser/Electron camera enumeration. If OBS has already loaded `v4l2loopback`, don't load another conflicting instance.

## 3. Configure the scene in OBS

1. Open **OBS Studio** in the same graphical desktop session as `Protonect`.
2. Add a **Window Capture** source. On a GNOME **X11** session, use the Xcomposite window-capture source and select the running `Protonect` preview window. On Wayland, you may need a **Screen Capture (PipeWire)** source instead of Window Capture.
3. Resize/crop the preview so Teams shows the desired RGB image rather than application decorations or depth panes. You may need a different Protonect view or a future direct RGB bridge for an ordinary full-frame webcam image.
4. In OBS, click **Start Virtual Camera** (in Controls, or via Tools depending on your version).
5. Confirm that OBS Virtual Camera is active. Its output should come from `/dev/video10` or whichever loopback node your system created.

The virtual camera carries **video only**. Select a normal microphone and speakers in Teams; libfreenect2 does not automatically provide Teams audio support.

## 4. Install Teams-for-Linux

Use the current download and packaging instructions from the independent project:

- https://github.com/IsmaelMartinez/teams-for-linux/releases — official project release packages, including `.deb`.
- https://github.com/IsmaelMartinez/teams-for-linux — README and documentation.

For a downloaded Debian `.deb` matching your architecture (usually amd64), change to the folder containing that file and run:

```bash
sudo apt install ./teams-for-linux_*.deb
```

Verify the `.deb` filename if the wildcard does not match. You can also follow the project's documented Debian repository/Flatpak methods. Flatpak/Snap editions may require explicit camera/device permissions before they can access OBS Virtual Camera.

Launch the application:

```bash
teams-for-linux
```

Sign in with the Teams account you normally use. You will need an internet connection to Microsoft's Teams service even though Kinect processing and the virtual camera remain local.

## 5. Select the virtual camera in Teams-for-Linux

1. Start OBS Virtual Camera **before** opening a Teams meeting. If necessary, fully quit and restart Teams-for-Linux after `/dev/video10` is created.
2. Open Teams **Settings → Devices** (or the camera selector on the meeting-join screen; the UI varies by Teams release).
3. Choose **OBS Virtual Camera** / **Kinect v2 (OBS Virtual Camera)** from the camera list.
4. Check the video preview, microphone and speaker selection before joining a real meeting.
5. When finished, stop the virtual camera in OBS, then stop Protonect.

## 6. Troubleshooting

| Symptom | Checks |
| --- | --- |
| `Protonect`: `LIBUSB_ERROR_ACCESS` | Kinect udev rule, device replug, `id -nG`, actual `/dev/bus/usb` access. |
| `Protonect`: no device | Kinect v2 only; powered adaptor; USB 3.x connection; `lsusb -t`. |
| OpenCL processor fails | Verify vendor OpenCL runtime via `clinfo`; test `Protonect gl` instead. |
| OBS source is black/blank | Preview window must remain open; select the correct window; try PipeWire Screen Capture on Wayland. |
| OBS Virtual Camera cannot start | `lsmod | grep v4l2loopback`, `v4l2-ctl --list-devices`, matching DKMS kernel headers, Secure Boot module signing. |
| Teams shows no camera | Start OBS virtual camera first; restart Teams; check Flatpak/Snap camera permissions if applicable. |
| Teams shows a depth map or a window border | Change/crop the OBS scene or modify the preview layout; this is window capture, not direct RGB capture. |
| No microphone audio | Choose a separate microphone through Teams/PipeWire; the virtual video camera does not supply audio. |

### Optional direction for future development

A real headless Kinect-to-V4L2 bridge could create a persistent V4L2 node and feed RGB frames directly from libfreenect2, removing the visible preview/OBS dependency. **That bridge is not included in the supplied ZIP or this fork's source changes.** It should be a separate future feature with working code and tests before documenting it as available.


## Experimental OBS-free alternative

This fork also contains an optional [direct colour-camera to V4L2 bridge](V4L2_KINECT_BRIDGE.md). It uses libfreenect2 + v4l2loopback and does not require a Protonect preview or OBS Studio. It is **experimental and needs testing on real hardware**; the OBS instructions above are the established fallback.

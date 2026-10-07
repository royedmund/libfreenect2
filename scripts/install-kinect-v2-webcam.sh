#!/usr/bin/env bash
# Reproducible experimental installation for Debian 13, Kinect v2 and V4L2.
# Source: https://github.com/royedmund/libfreenect2
# Run as a regular desktop user from the root of the cloned fork:
#   ./scripts/install-kinect-v2-webcam.sh
set -Eeuo pipefail

say() { printf '\n==> %s\n' "$*"; }
fail() { printf '\nERROR: %s\n' "$*" >&2; exit 1; }
usage() {
  cat <<'USAGE'
Usage: ./scripts/install-kinect-v2-webcam.sh [--no-start] [--replace-existing]

On Debian 13, build this fork into ~/.local, compile the hardware-tested
RGB24 bridge, configure access to Kinect v2 + /dev/video10, and install a
user systemd service. Does not install Teams-for-Linux; see docs guide.

--no-start          Install and enable nothing until you verify manually.
--replace-existing  Explicitly allow replacing an installed bridge/service.

Do NOT run with sudo. The script invokes sudo for packages, udev and kernel
module configuration only. Your existing OpenKinect tree is not modified.
USAGE
}

NO_START=false
REPLACE=false
for arg in "$@"; do
  case "$arg" in
    --no-start) NO_START=true ;;
    --replace-existing) REPLACE=true ;;
    -h|--help) usage; exit 0 ;;
    *) usage >&2; fail "Unknown argument: $arg" ;;
  esac
done

[[ $EUID -ne 0 ]] || fail 'Run as a normal desktop user, not root/sudo.'
command -v sudo >/dev/null || fail 'sudo is needed for packages and device setup.'
command -v systemctl >/dev/null || fail 'systemd is required for the supplied service.'
command -v apt-get >/dev/null || fail 'This installer targets Debian 13 with APT.'
[[ -f /etc/debian_version ]] || fail 'Only Debian is supported by this installer.'
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
[[ -f "$repo/tools/kinect-v4l2-bridge/kinect-v4l2-bridge.cpp" ]] || fail 'Bridge source not found. Run this from a complete copy of the fork.'
[[ -f "$repo/CMakeLists.txt" ]] || fail 'Project root is missing CMakeLists.txt.'
[[ -n "${XDG_RUNTIME_DIR:-}" ]] || fail 'Run from a logged-in user session so systemctl --user works.'

unit="$HOME/.config/systemd/user/kinect-v2-webcam.service"
binary="$HOME/.local/bin/kinect-v2-bridge"
if [[ "$REPLACE" != true ]] && { [[ -e "$unit" ]] || [[ -e "$binary" ]]; }; then
  fail "Existing bridge detected. Nothing changed. Inspect '$unit' and '$binary' first; rerun with --replace-existing only if you intend to replace it."
fi

if [[ -e /dev/video10 ]] && ! v4l2-ctl -d /dev/video10 --all 2>/dev/null | grep -qi 'v4l2 loopback'; then
  # If v4l2-ctl isn't installed yet, postpone this check until after APT.
  if command -v v4l2-ctl >/dev/null 2>&1; then
    fail '/dev/video10 is present but not a v4l2loopback webcam; do not overwrite it.'
  fi
fi

say 'Installing build tools and multimedia dependencies (requires sudo)'
sudo apt-get update
packages=(build-essential cmake pkg-config git
          libusb-1.0-0-dev libturbojpeg0-dev libglfw3-dev libgl-dev
          libjpeg-dev usbutils v4l2loopback-dkms v4l2loopback-utils
          v4l-utils)
# Install matching headers first, before v4l2loopback-dkms builds its module.
# A nonstandard kernel must supply its own headers; never install mismatched ones.
if [[ ! -e "/lib/modules/$(uname -r)/build" ]]; then
  sudo apt-get install -y "linux-headers-$(uname -r)" || fail 'Missing matching headers for the running kernel.'
fi
[[ -e "/lib/modules/$(uname -r)/build" ]] || fail 'Matching kernel headers missing. Install your running kernel headers first.'
sudo apt-get install -y "${packages[@]}"

say 'Checking /dev/video10 before creating the loopback'
if [[ -e /dev/video10 ]]; then
  v4l2-ctl -d /dev/video10 --all | grep -qi 'v4l2 loopback' || fail '/dev/video10 belongs to another driver.'
  if sudo fuser /dev/video10 >/dev/null 2>&1; then
    fail 'Another process already has /dev/video10 open. Stop OBS/any other bridge before installing.'
  fi
fi

say 'Configuring restricted Kinect USB access for the video group'
if [[ ! -e /etc/udev/rules.d/90-kinect2-local.rules ]]; then
  sudo tee /etc/udev/rules.d/90-kinect2-local.rules >/dev/null <<'RULES'
# libfreenect2 Kinect v2; assumes the interactive user is in group video.
SUBSYSTEM=="usb", ATTR{idVendor}=="045e", ATTR{idProduct}=="02c4", GROUP="video", MODE="0660", TAG+="uaccess"
SUBSYSTEM=="usb", ATTR{idVendor}=="045e", ATTR{idProduct}=="02d8", GROUP="video", MODE="0660", TAG+="uaccess"
SUBSYSTEM=="usb", ATTR{idVendor}=="045e", ATTR{idProduct}=="02d9", GROUP="video", MODE="0660", TAG+="uaccess"
RULES
else
  say 'Existing local Kinect rule found; leaving it unchanged'
fi
sudo udevadm control --reload-rules
if ! id -nG | tr ' ' '\n' | grep -qx video; then
  say "Adding $USER to video group; re-login will be needed"
  sudo usermod -aG video "$USER"
fi

say 'Configuring v4l2loopback /dev/video10 for the next boot'
modconf=/etc/modprobe.d/kinect-v2-webcam.conf
loadconf=/etc/modules-load.d/kinect-v2-webcam.conf
if [[ -f "$modconf" ]]; then
  if ! grep -q 'video_nr=10' "$modconf"; then
    fail "$modconf already exists with different settings. Review it manually."
  fi
else
  printf '%s\n' 'options v4l2loopback devices=1 video_nr=10 card_label="Microsoft Kinect v2" exclusive_caps=1' |
    sudo tee "$modconf" >/dev/null
fi
if [[ ! -e "$loadconf" ]]; then
  printf 'v4l2loopback\n' | sudo tee "$loadconf" >/dev/null
fi
if ! [[ -e /dev/video10 ]]; then
  if lsmod | grep -q '^v4l2loopback '; then
    say 'v4l2loopback was already loaded differently. A reboot is needed to create /dev/video10.'
  else
    sudo modprobe v4l2loopback || fail 'Cannot load v4l2loopback. Inspect dkms status, kernel headers, and Secure Boot.'
  fi
fi

say 'Building libfreenect2 into ~/.local (no sudo)'
libbuild="$repo/build-kinect-webcam"
cmake -S "$repo" -B "$libbuild" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$HOME/.local" \
  -DENABLE_OPENCL=OFF -DENABLE_CUDA=OFF -DENABLE_OPENGL=ON \
  -DBUILD_OPENNI2_DRIVER=OFF
cmake --build "$libbuild" --parallel 4
cmake --install "$libbuild"

say 'Building RGB24 bridge against the installed libfreenect2'
export PKG_CONFIG_PATH="$HOME/.local/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
pkg-config --modversion freenect2 || fail 'No freenect2.pc at ~/.local/lib/pkgconfig. Inspect CMake installation.'
bridgebuild="$repo/build-kinect-webcam-bridge"
cmake -S "$repo/tools/kinect-v4l2-bridge" -B "$bridgebuild" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_BUILD_RPATH="$HOME/.local/lib" \
  -DCMAKE_INSTALL_RPATH="$HOME/.local/lib"
cmake --build "$bridgebuild" --parallel 4
mkdir -p "$HOME/.local/bin" "$HOME/.config/systemd/user"

if [[ "$REPLACE" == true ]]; then
  systemctl --user disable --now kinect-v2-webcam.service 2>/dev/null || true
  if [[ -e "$binary" ]]; then
    cp -p "$binary" "$binary.backup-$(date +%Y%m%d-%H%M%S)"
  fi
  if [[ -e "$unit" ]]; then
    cp -p "$unit" "$unit.backup-$(date +%Y%m%d-%H%M%S)"
  fi
fi
install -m 0755 "$bridgebuild/kinect-v2-bridge" "$binary"
install -m 0644 "$repo/systemd/user/kinect-v2-webcam.service" "$unit"

say 'Confirming that the installed binary finds ~/.local/lib/libfreenect2.so'
ldd "$binary" | grep 'libfreenect2' || true
if ldd "$binary" | grep -q 'not found'; then
  fail 'The bridge still has unresolved library dependencies. Review RUNPATH with readelf -d ~/.local/bin/kinect-v2-bridge.'
fi

systemctl --user daemon-reload
if [[ "$NO_START" == true ]]; then
  say 'Files installed. User service was NOT enabled or started (--no-start).'
elif ! id -nG | tr ' ' '\n' | grep -qx video; then
  say 'Installed, but NOT started: log out and in so video group membership takes effect.'
  echo 'Then: systemctl --user enable --now kinect-v2-webcam.service'
elif [[ ! -e /dev/video10 ]]; then
  say 'Installed, but NOT started: reboot to create /dev/video10.'
  echo 'Then: systemctl --user enable --now kinect-v2-webcam.service'
else
  say 'Enabling and starting user service'
  systemctl --user enable --now kinect-v2-webcam.service
  systemctl --user status kinect-v2-webcam.service --no-pager -l || true
fi

cat <<'DONE'

Installation steps finished. Verification:
  lsusb -t
  v4l2-ctl --list-devices
  systemctl --user status kinect-v2-webcam.service --no-pager
  journalctl --user -u kinect-v2-webcam.service -n 30 --no-pager
  v4l2-ctl -d /dev/video10 --stream-mmap --stream-count=90 --stream-poll

Open docs/V4L2_KINECT_BRIDGE.md for Teams-for-Linux instructions,
reproducibility checks, troubleshooting and uninstall details.
DONE

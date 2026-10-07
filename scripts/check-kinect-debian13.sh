#!/usr/bin/env bash
set -u

# Read-only diagnostics for Debian 13 + Kinect for Windows v2.
# No sudo, driver changes, udev changes, or package installation.

printf 'libfreenect2 Debian/Kinect v2 diagnostic report\n'
printf '==========================================\n'
printf 'Date: %s\n' "$(date --iso-8601=seconds)"
printf 'Host: %s\n' "$(hostname)"
printf 'Kernel: %s\n' "$(uname -r)"
printf 'User: %s\n' "$(id -un)"
printf 'Groups: %s\n' "$(id -nG)"

report() {
  printf '\n== %s ==\n' "$1"
}

report 'USB devices'
if command -v lsusb >/dev/null 2>&1; then
  lsusb | grep -Ei '045e:02c4|045e:02d8|045e:02d9|Microsoft|Kinect' || echo 'No Kinect identified by lsusb (check connection/power).'
else
  echo 'lsusb is missing (package: usbutils).'
fi

report 'USB transport topology'
if command -v lsusb >/dev/null 2>&1; then lsusb -t; fi

report 'Relevant build/runtime tools'
for p in cmake pkg-config clinfo obs teams-for-linux v4l2-ctl; do
  if command -v "$p" >/dev/null 2>&1; then
    printf '%-18s %s\n' "$p" "$(command -v "$p")"
  else
    printf '%-18s %s\n' "$p" 'not found'
  fi
done

report 'V4L2 devices (if any)'
if command -v v4l2-ctl >/dev/null 2>&1; then
  v4l2-ctl --list-devices 2>&1 || true
else
  ls -l /dev/video* 2>/dev/null || echo 'No /dev/video* nodes found.'
fi

report 'v4l2loopback kernel module'
if [[ -r /proc/modules ]] && command -v lsmod >/dev/null 2>&1; then
  lsmod | grep -E '^v4l2loopback[[:space:]]' || echo 'Not loaded.'
fi

report 'Installed USB permissions rules'
for rule in /etc/udev/rules.d/90-kinect2-local.rules /etc/udev/rules.d/90-kinect2.rules; do
  if [[ -r "$rule" ]]; then printf 'Present: %s\n' "$rule"; fi
done

report 'Protonect binary'
if [[ -x ./build/bin/Protonect ]]; then
  printf 'Present: %s/build/bin/Protonect\n' "$PWD"
else
  echo 'No executable ./build/bin/Protonect in this working directory.'
fi

printf '\nEnd of report. Do not paste personal credentials or unrelated logs publicly.\n'

# Fork changes compared with OpenKinect/libfreenect2

## Provenance

- **Upstream repository:** https://github.com/OpenKinect/libfreenect2
- **Reference revision in the supplied ZIP:** `fd64c5d9b214df6f6a55b4419357e51083f15d93` (`master`, tag `v0.2.1`).
- **Fork maintainer's GitHub account:** `royedmund`.
- **Target platform for these additional notes:** Debian 13 (Trixie), x86_64, Kinect for Windows v2.
- **Upstream ownership, history and license:** retained. This is an independent community fork, not an official OpenKinect release.

## Source-code differences present in the uploaded project

Exactly two C++ files contain substantive changes relative to the archived upstream commit:

| File | Original | Updated | Scope |
| --- | --- | --- | --- |
| `src/opencl_depth_packet_processor.cpp` | Local constant `CL_ICDL_VERSION` | `ICDL_VERSION_REQUEST` | Rename definition and its use when calling `clGetICDLoaderInfoOCLICD(...)` |
| `src/opencl_kde_depth_packet_processor.cpp` | Local constant `CL_ICDL_VERSION` | `ICDL_VERSION_REQUEST` | Same rename in the KDE depth processing path |

In both files, the value remains **2**. No depth algorithm, frame format, USB transfer handling, OpenCL kernel, or public API has changed. The rename is intended to avoid collisions or ambiguity with identifiers supplied by OpenCL headers. It should not be described as an independently benchmarked performance enhancement.

The ZIP also contained nonfunctional packaging differences (four dependency shell scripts had lost their executable bits), two `.bak` copies of the pre-change C++ files, a `.git/` directory and `build/` artefacts. **Those are deliberately excluded from the publishable changes**; the upstream executable modes are retained.

## Documentation supplied by this fork

- `docs/DEBIAN13_SETUP.md` — Debian 13 dependencies, USB permissions, build and functional tests.
- `docs/TEAMS_FOR_LINUX.md` — use Kinect v2 as a video source in Teams-for-Linux by way of OBS Studio's virtual camera.
- `docs/PUBLISH_FORK.md` — preserving upstream history and publishing the small patch to a genuine GitHub fork.
- `scripts/check-kinect-debian13.sh` — read-only preflight diagnostics (does not install drivers or alter system configuration).

**Important distinction:** The accompanying Teams-for-Linux documentation describes a separate *application workflow*, not a new camera bridge implemented in libfreenect2. The uploaded ZIP contains no native V4L2 Kinect webcam bridge, OBS plug-in or Teams integration source code. Do not advertise direct `/dev/video10` output from libfreenect2 alone.

## Validation and limitations

- Inspection of the supplied ZIP confirms the two OpenCL identifier renames.
- The ZIP includes locally generated CMake/build artefacts and a `Protonect` binary, but those are **not reproducible test evidence** for arbitrary systems and are excluded from the fork publication.
- Prior maintainer experimentation established a usable Kinect preview on Debian and a working OBS-based Teams workflow on the maintainer's own machine. This release was **not built or device-tested in the packaging environment**.
- This is not a general modernization of the upstream library; the remaining upstream README, code and licensing notices are preserved.

## Licenses and attribution

Preserve the upstream source files and their headers, `APACHE20`, `GPL2`, and `CONTRIB` when publishing or redistributing the project. Follow the per-file licensing information in the original source. The fork's documentation does not replace upstream licensing terms.


## Subsequent V4L2 bridge addition

The source-code comparison above describes the original uploaded ZIP. Later, this fork added experimental files:

- `tools/kinect-v4l2-bridge/kinect-v4l2-bridge.cpp`: colour-only userspace bridge that converts libfreenect2 colour frames to YUYV422 and feeds a v4l2loopback device.
- `tools/kinect-v4l2-bridge/CMakeLists.txt`: standalone optional example build.
- `docs/V4L2_KINECT_BRIDGE.md`: Debian 13 / Teams-for-Linux setup and limitations.

No upstream libfreenect2 processing routines are changed by this later addition. This is not a native kernel webcam driver. It has had a compile-time syntax check, but requires physical device tests before being considered proven.


## Hardware-tested RGB24 webcam update (7 October 2026)

**Important distinction:** the source-code diff described above refers to the original uploaded
fork; subsequent features were added separately. The initially published standalone Kinect
V4L2 example used YUYV422 and was **experimental**. On 7 October 2026, the maintainer provided
the existing **RGB24** source from an actually running Debian 13.7/X570 system whose
`/dev/video10` was successfully used in Teams-for-Linux. We replaced the YUYV example
with that observed RGB24 source and recorded the results in
[`docs/TESTED_X570_2026-10-07.md`](TESTED_X570_2026-10-07.md).

Added/updated files: `tools/kinect-v4l2-bridge/kinect-v4l2-bridge.cpp`, its standalone
CMake file, `scripts/install-kinect-v2-webcam.sh`, `systemd/user/kinect-v2-webcam.service`,
and `docs/V4L2_KINECT_BRIDGE.md`. These additions are **not** from upstream OpenKinect.
The original runtime linked directly to the X570's build tree; the new installer uses a
per-user `~/.local` library instead. That installer and packaging flow are **not yet
validated on a clean second machine**.

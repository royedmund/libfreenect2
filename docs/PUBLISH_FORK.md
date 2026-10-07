# Maintain and update this libfreenect2 fork

The fork is already published at [royedmund/libfreenect2](https://github.com/royedmund/libfreenect2), with [OpenKinect/libfreenect2](https://github.com/OpenKinect/libfreenect2) as its upstream. Use this guide for later updates. The initial ZIP-to-fork publication procedure is no longer a prerequisite.

## 1. Obtain a clean checkout

```bash
git clone https://github.com/royedmund/libfreenect2.git ~/libfreenect2-review
cd ~/libfreenect2-review
git switch -c docs/your-update
```

If that destination already exists, use your existing checkout or choose another directory. In an existing checkout, inspect `git status` before pulling or switching branches so local work is not lost.

## 2. Understand the scope

Read [CHANGES_FROM_UPSTREAM.md](CHANGES_FROM_UPSTREAM.md). The fork retains upstream source and history, adds two original OpenCL identifier renames, and later adds a separate optional RGB24 V4L2 bridge with installer/service files. These are distinct changes.

| Change | Relevant locations |
| --- | --- |
| Core compatibility edits | The two OpenCL depth processor files listed in the change record |
| Direct webcam integration | `tools/kinect-v4l2-bridge/`, `scripts/install-kinect-v2-webcam.sh`, `systemd/user/` |
| Setup and observed results | `docs/`, plus the fork introduction in `README.md` |

Preserve `APACHE20`, `GPL2`, `CONTRIB` and original per-file notices. Do not commit build directories, local backups or installed binaries.

## 3. Validate the actual change

For documentation-only edits:

```bash
git diff --check
git diff --stat
git diff
```

Verify relative links, filenames, service names and commands against the checkout. For script edits, also run:

```bash
bash -n scripts/check-kinect-debian13.sh
bash -n scripts/install-kinect-v2-webcam.sh
```

For library/bridge changes, follow [DEBIAN13_SETUP.md](DEBIAN13_SETUP.md) and [V4L2_KINECT_BRIDGE.md](V4L2_KINECT_BRIDGE.md). A successful syntax/build check is not a hardware test. Record new device-test results in a dated document, using [the X570 record](TESTED_X570_2026-10-07.md) as a reference. Keep the clean-install status explicit until that workflow has been reproduced on a clean machine.

## 4. Commit and publish for review

Stage only the files you intended to change. For example, for a README edit:

```bash
git add README.md
git diff --cached --check
git diff --cached
git commit -m 'Clarify fork setup and documentation navigation'
git push -u origin HEAD
```

Open a pull request into this fork's `master` branch. Describe the change, checks performed and any hardware tests still outstanding. Use Git Credential Manager, an SSH key or another supported credential method; do not put credentials into scripts or documentation.

## 5. Review upstream updates separately

If the upstream remote is not already configured:

```bash
git remote add upstream https://github.com/OpenKinect/libfreenect2.git
```

Then inspect updates before deciding to merge them:

```bash
git fetch upstream
git log --oneline HEAD..upstream/master
git diff HEAD...upstream/master
```

Review compatibility with the fork's OpenCL changes and optional bridge. Retest before publishing any new hardware-support or performance claims.

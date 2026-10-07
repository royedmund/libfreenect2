# Publish a genuine libfreenect2 fork on GitHub

The supplied ZIP includes the *original* OpenKinect Git history (`.git/`) based on commit `fd64c5d9b214df6f6a55b4419357e51083f15d93`, but it is **not yet a GitHub fork**. A proper GitHub fork preserves the upstream relationship and contributors' history.

## 1. Create a fork through GitHub

1. Sign in to GitHub as **`royedmund`**.
2. Open https://github.com/OpenKinect/libfreenect2 .
3. Click **Fork** → choose **`royedmund`** as owner.
4. Use repository name `libfreenect2` (or choose another name and update the commands below).
5. Choose a **full fork** if offered an option to copy only the default branch; this keeps tags/branches where available.
6. Ensure your new fork is accessible at `https://github.com/royedmund/libfreenect2`.

The packaged files cannot themselves create a fork on GitHub. The account owner needs to complete that UI step; the prepared patch supplies the source changes and documentation.

## 2. Obtain the patch produced from the supplied project

Download **`libfreenect2-fork-update.patch`** from the deliverables. Place it in `~/Downloads` on your Debian computer.

## 3. Clone *your fork*, check compatibility and apply the patch

```bash
cd ~
git clone https://github.com/royedmund/libfreenect2.git
cd libfreenect2

git status --short
git log -1 --oneline

git apply --check ~/Downloads/libfreenect2-fork-update.patch
git apply ~/Downloads/libfreenect2-fork-update.patch

git diff --stat
git diff --check
```

If the `git apply --check` step fails, **stop there**. Your fork may have a different upstream revision, so inspect and reconcile rather than blindly overwriting source files. The patch was generated against the revision supplied in the ZIP.

## 4. Review before publication

Review the actual patch:

```bash
git diff -- src/opencl_depth_packet_processor.cpp src/opencl_kde_depth_packet_processor.cpp
git status --short
```

The patch should contain **two small OpenCL edits, documentation and a read-only diagnostic script**, not generated build files, backups or a `.git/` copy. Upstream `APACHE20`, `GPL2`, `CONTRIB`, source headers and commit history remain intact.

## 5. Commit and push

```bash
git add README.md docs/ scripts/check-kinect-debian13.sh \
        src/opencl_depth_packet_processor.cpp \
        src/opencl_kde_depth_packet_processor.cpp

git commit -m 'Document Debian 13 and Teams workflow; clarify OpenCL ICD version identifier'
git push origin master
```

GitHub does **not** accept account passwords for Git operations over HTTPS. Use Git Credential Manager, `gh auth login`, or an appropriate credential/token method; never paste authentication secrets into project files.

## 6. Validate the result on GitHub

Check that the fork's **README** links to the new documentation, that `docs/CHANGES_FROM_UPSTREAM.md` accurately explains the two actual source edits, and that `build/`, `*.bak`, and `.git/` were not committed. Build the fork on a clean Debian installation before publishing a release that claims tested binaries.

To follow ongoing upstream changes later, add the upstream remote and *review* changes before merging:

```bash
git remote add upstream https://github.com/OpenKinect/libfreenect2.git
git fetch upstream
```

Do not automatically merge upstream without reviewing any new changes against your OpenCL compatibility tweak.

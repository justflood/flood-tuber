# Arch Linux / CachyOS AUR Packaging for Flood Tuber

This directory contains `PKGBUILD` definitions for Arch Linux, CachyOS, and other Arch-based distributions.

## Packages

- **`PKGBUILD`**: Builds from release tarballs (for tagged releases, e.g. `obs-plugin-flood-tuber`).
- **`PKGBUILD-git`**: VCS package building from the latest `master` git commit (`obs-plugin-flood-tuber-git`).

## Testing / Building Locally

On Arch Linux or CachyOS:

1. Clone or copy `PKGBUILD`:
   ```bash
   cd packaging/aur
   ```

2. Generate updated checksums (for release PKGBUILD):
   ```bash
   updpkgsums
   ```

3. Build and install:
   ```bash
   makepkg -si
   ```

## Publishing to the AUR

To publish on https://aur.archlinux.org:

1. Clone the AUR repository:
   ```bash
   git clone ssh://aur@aur.archlinux.org/obs-plugin-flood-tuber.git
   cd obs-plugin-flood-tuber
   ```

2. Copy `PKGBUILD` into the repository:
   ```bash
   cp /path/to/packaging/aur/PKGBUILD .
   ```

3. Generate `.SRCINFO`:
   ```bash
   makepkg --printsrcinfo > .SRCINFO
   ```

4. Commit and push:
   ```bash
   git add PKGBUILD .SRCINFO
   git commit -m "Initial release v1.2.0"
   git push origin master
   ```

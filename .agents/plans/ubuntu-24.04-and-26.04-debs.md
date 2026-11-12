# Plan: Ubuntu 24.04 and 26.04 `.deb` support

Status: in progress on `feat/ubuntu-24-04-deb`.

## Goal

Ship a `.deb` for each supported Ubuntu LTS, so apt OBS users get a native plugin
on both 24.04 and 26.04, while everything else keeps the Flatpak bundle.

| Release | OBS from the PPA | FFmpeg | Qt |
|---|---|---|---|
| 24.04 | 32.2.0 | 6.1 | 6.4 |
| 26.04 | 33.0.0 | 8.0 | 6.10 |

The 24.04 PPA is frozen at OBS 32.2.0, so 24.04 users who want OBS 33 use Flatpak
OBS and the `.flatpak`; 26.04 users get OBS 33 from apt.

## Decision: no `buildspec.json` rollback

`buildspec.json` stays at OBS 33.0.1. Linux never builds against it: the Linux
build uses the OBS the PPA installs, and each `.deb` declares its own OBS version
from the installed `libobs`. The only buildspec effect is the configure warning,
which we handle by adding `dependencies.obs-studio.linuxSupported`.

No source changes are needed: the OBS-32-only code paths are already
runtime-detected, and the FFmpeg API used exists in 6.1 and 8.0.

## Naming

`replay-buffer-pro-<version>-x86_64-ubuntu<release>-linux-gnu.deb` (and `.ddeb`),
matching Advanced Scene Switcher's `…-x86_64-ubuntu24.04-linux-gnu.deb`. The
package name stays `replay-buffer-pro`, so `apt remove replay-buffer-pro` keeps
working.

## Changes

### 1. Build system
- `cmake/linux/defaults.cmake`
  - Inject `ubuntu<release>` from `/etc/os-release` into `CPACK_PACKAGE_FILE_NAME`.
  - Compare the installed OBS against `linuxSupported` instead of the single
    `buildspec.json` version.
- `buildspec.json`
  - Add `dependencies.obs-studio.linuxSupported: ["32.2", "33.0"]`.

### 2. CI
- `.github/workflows/build-project.yaml`
  - `ubuntu-build` matrix: `os: [ubuntu-24.04, ubuntu-26.04]`.
  - Upload the source tarball only from the 26.04 leg (the flatpak build uses it).
- `.github/workflows/push.yaml`
  - `ubuntu_candidates` covers both `ubuntu-24.04-x86_64-*` and `ubuntu-26.04-x86_64-*`.

### 3. Release
- `.agents/skills/release/scripts/compose-release-body.sh`
  - Ubuntu section: two `.deb`s, one per release.
- `.agents/skills/release/SKILL.md`
  - Expect both `.deb`s and both `.ddeb`s.

### 4. Site
- `site/src/components/DownloadSplit.astro`
  - Menu: Ubuntu 24.04 and Ubuntu 26.04; asset matchers.
- `site/src/pages/index.astro`
  - Install/uninstall tabs, spec lines, FAQ, JSON-LD.
- `site/src/content/llms.txt`
  - Mirror the FAQ and platform lines.

### 5. Docs
- `README.md`, `AGENTS.md`, `docs/architecture/build-and-localization.md`,
  `site/src/content/pages/compare.md`, `obs-replay-buffer-guide.md`.

### 6. Test desktop
- `tools/linux-desktop/Dockerfile`: `ARG UBUNTU=26.04`.
- `tools/linux-desktop/run.sh`: `UBUNTU` env var, image name, `.deb` glob.
- `tools/linux-desktop/entrypoint.sh`: install the `.deb` matching the image's
  `/etc/os-release`.

## Verification
- `tools/linux-desktop/run.sh deb` builds the `.deb` for both releases;
  `dpkg-deb -f` shows the expected `Depends:` (OBS 32.2 / 33.0, FFmpeg 6.1 / 8.0).
- CI's `ubuntu-build` matrix builds both legs.
- `site/` `npm run check` and `npm run build`.

## Rollback
- The `buildspec.json` change is additive; no rollback needed.
- If the 24.04 build breaks, drop the `ubuntu-24.04` matrix leg and the
  `linuxSupported` entry; the rest of the change is unaffected.

# Build, Packaging, and Localization

This document describes how the plugin is built, packaged, and localized.

## Build system (CMake)

The build system follows the [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate) structure.

- Project name and version are defined in `buildspec.json` (not directly in `CMakeLists.txt`).
- C++ standard: C++17.
- Sources: module entry, plugin, managers, UI, and utilities.
- Links against OBS (libobs, obs-frontend-api), Qt6 (Widgets/Core), FFmpeg libraries (avformat, avcodec, avutil), and libcurl (system library on macOS and Linux, obs-deps on Windows) for the About page's update check.

### Key files

| File | Purpose |
|---|---|
| `buildspec.json` | Plugin metadata (name, version, author) and pinned dependency versions with SHA256 hashes (Windows + macOS; Linux uses system packages) |
| `CMakePresets.json` | Configure/build presets for Windows x64, macOS universal and Ubuntu x86_64 (local and CI) |
| `CMakeLists.txt` | Main build file — follows the template pattern |
| `cmake/common/bootstrap.cmake` | Entry point: reads `buildspec.json`, sets plugin variables, blocks in-source builds |
| `cmake/common/osconfig.cmake` | OS detection, sets `OS_WINDOWS`/`OS_MACOS`/`OS_LINUX`, adds platform-specific module path |
| `cmake/common/buildnumber.cmake` | Incremental build number counter |
| `cmake/common/compiler_common.cmake` | C/C++17 standard, visibility presets, clang/AppleClang warning flags |
| `cmake/common/buildspec_common.cmake` | Generic dependency downloader: fetches/extracts archives, builds OBS from source (Windows and macOS paths) |
| `cmake/common/helpers_common.cmake` | Shared helper functions |
| `cmake/common/ccache.cmake` | Generic ccache support module, used by the Linux build (`ENABLE_CCACHE` in the `ubuntu-ci-x86_64` preset). macOS CI uses Xcode's built-in compilation cache instead; see below |
| `cmake/windows/buildspec.cmake` | Windows platform slice for dependency setup |
| `cmake/windows/compilerconfig.cmake` | MSVC-specific compiler flags (`/W3`, `/utf-8`, `/permissive-`, LTO) |
| `cmake/windows/defaults.cmake` | Sets install prefix to `%ALLUSERSPROFILE%/obs-studio/plugins` |
| `cmake/windows/helpers.cmake` | `set_target_properties_plugin()`: install rules, rundir post-build copy, `.rc` resource |
| `cmake/windows/resources/resource.rc.in` | Windows VERSIONINFO resource template embedded in the DLL |
| `cmake/macos/buildspec.cmake` | macOS platform slice: sets `arch=universal`, `platform=macos`, calls `_check_dependencies_macos()`, clears quarantine |
| `cmake/macos/compilerconfig.cmake` | Requires Xcode generator, checks SDK ≥ 26.5 / Xcode ≥ 26.5, sets dSYM release flags |
| `cmake/macos/defaults.cmake` | Sets install prefix to `~/Library/Application Support/obs-studio/plugins`, RPATH settings |
| `cmake/macos/helpers.cmake` | `set_target_properties_plugin()`: bundle properties, Xcode attributes, rundir copy, pkgbuild/productbuild packaging |
| `cmake/macos/xcode.cmake` | Full Xcode attribute configuration: codesigning, hardened runtime, dSYM, warning flags |
| `cmake/macos/resources/distribution.in` | XML installer distribution definition for `productbuild` |
| `cmake/macos/resources/create-package.cmake.in` | CMake install script that runs `pkgbuild` + `productbuild` to produce a `.pkg` |
| `cmake/linux/compilerconfig.cmake` | GCC/Clang warning flags, ccache, OpenMP SIMD |
| `cmake/linux/defaults.cmake` | GNUInstallDirs, CPack (`.deb`, `-dbgsym.ddeb`, source `.tar.xz` into `release/`), and a fallback `find_package` shim for OBS installs without CMake packages |
| `cmake/linux/helpers.cmake` | `set_target_properties_plugin()`: `.so` without `lib` prefix, install to `lib/<arch>-linux-gnu/obs-plugins/` and `share/obs/obs-plugins/<name>/`, rundir copy |

### Dependency management

Dependencies are auto-downloaded at CMake configure time into `.deps/`:
- **OBS Studio source** — built from source at configure time (libobs + obs-frontend-api)
- **Prebuilt obs-deps** — pre-compiled FFmpeg, and other OBS dependencies
- **Prebuilt Qt6** — pre-compiled Qt6 for MSVC

Versions and SHA256 hashes are pinned in `buildspec.json` under `dependencies.<name>.hashes` for both `windows-x64` and `macos`. The `cmake/common/buildspec_common.cmake` module handles downloading, hash verification, extraction, and building OBS.

Linux follows the template and downloads nothing: there is no `cmake/linux/buildspec.cmake`. The build uses system packages that `.github/scripts/utils.zsh/setup_ubuntu` installs with apt: `obs-studio` from `ppa:obsproject/obs-studio` (it ships the libobs and obs-frontend-api headers and CMake packages), Qt6, FFmpeg (`libavformat-dev`, `libavcodec-dev`, `libavutil-dev`) and `libcurl4-openssl-dev`. The plugin is built against whatever OBS version the PPA carries, and against Ubuntu's FFmpeg, which is what that OBS links. `setup_ubuntu` limits the PPA source to `Architectures: amd64`, because Launchpad advertises an empty amd64v3 index for PPAs (LP#2127888), and on a host using Ubuntu 26.04's amd64v3 packages, such as GitHub's `ubuntu-26.04` runner, apt would otherwise install Ubuntu's own older OBS instead.

### Windows build

```bash
# Configure (first run downloads deps and builds OBS — takes a few minutes)
cmake --preset windows-x64

# Build
cmake --build --preset windows-x64

# Install to OBS plugin folder (%ALLUSERSPROFILE%/obs-studio/plugins/)
cmake --install build_x64 --config RelWithDebInfo
```

The configure step:
1. Downloads prebuilt obs-deps and Qt6 from GitHub releases
2. Downloads OBS Studio source archive
3. Configures and builds libobs + obs-frontend-api from source
4. Sets `CMAKE_PREFIX_PATH` so `find_package()` resolves all dependencies
5. FFmpeg libraries (avformat, avcodec, avutil) are found from the prebuilt obs-deps archive

### macOS build

Requires Xcode 26.5+ and macOS SDK 26.5+.

```bash
# Configure (first run downloads deps and builds OBS — takes a few minutes)
cmake --preset macos

# Build (universal binary: arm64 + x86_64, deployment target macOS 13.0)
cmake --build --preset macos

# Install to ~/Library/Application Support/obs-studio/plugins/
cmake --install build_macos --config RelWithDebInfo
```

The configure step is identical in structure to Windows: downloads obs-deps, Qt6, and OBS Studio source, then builds OBS. The `cmake/macos/buildspec.cmake` sets `arch=universal` and `platform=macos`, and removes macOS quarantine attributes from downloaded archives.

The build produces a `.plugin` bundle at `build_macos/rundir/<config>/replay-buffer-pro.plugin` for quick testing.

### Linux build

Requires Ubuntu 24.04 or 26.04 x86_64, zsh, and sudo for apt.

```bash
# Install dependencies with apt, configure (Ninja, prefix /usr), build, and stage into release/RelWithDebInfo/
.github/scripts/build-ubuntu

# Or, once the dependencies are installed
cmake --preset ubuntu-x86_64
cmake --build --preset ubuntu-x86_64
sudo cmake --install build_x86_64
```

The Ubuntu presets install to `/usr`, next to OBS from apt. `build-ubuntu` picks the `ubuntu-ci-x86_64` preset (warnings as errors, ccache) when `CI` is set, as it is on GitHub Actions.

### Linux test desktop

`tools/linux-desktop/` builds and runs the Linux plugin on any machine with Docker, including Apple Silicon Macs. Its image is Ubuntu 24.04 or 26.04 x86_64 (default 26.04) with OBS Studio from the PPA, the packages `build-ubuntu` needs, and a desktop served to the browser by Xvfb, Openbox, x11vnc and noVNC.

- `run.sh deb` configures with the `ubuntu-x86_64` preset and runs CPack's `package` target in the image for the image's Ubuntu release, writing the `.deb` to `release/`. Each release builds in its own `build_linux-desktop-<release>/build/`, not the preset's `build_x86_64/`: Ninja doesn't notice another release's older headers and libraries, so a shared folder would package the wrong release's binary.
- `run.sh up` starts the desktop on `localhost:6080` (obs-websocket on `4455`). The entrypoint installs the newest matching `release/*.deb` with `dpkg -i` on every start, and OBS restarts when it quits.
- OBS settings and logs persist in `build_linux-desktop-<release>/config/` and recordings in `build_linux-desktop-<release>/recordings/`. A fresh config is seeded from `tools/linux-desktop/obs-config/`: no first-run wizard, 720p30, Advanced output with x264 `ultrafast` and 2-second keyframes, MKV to `/recordings`, Replay Buffer on at 60 seconds, and a scene of FFmpeg `lavfi` test sources (`testsrc2` video with a timecode, a `sine` tone).

It checks behaviour, not performance: x86 is emulated and OBS renders with Mesa's software rasterizer. The display is X11 only.

### Flatpak bundle

Flatpak OBS loads plugins only as Flatpak extensions of `com.obsproject.Studio`, so the plugin is also packaged as the extension `com.obsproject.Studio.Plugin.ReplayBufferPro` and shipped on each release as `replay-buffer-pro-<version>-x86_64.flatpak`, which users install with `flatpak install --user`. `tools/flatpak/` holds the manifest, metainfo, `build-bundle.sh` and a Docker setup that builds and runs it (`tools/flatpak/README.md`).

- `build-bundle.sh` builds from a source `.tar.xz`, never a git checkout. It fills the manifest's `@SOURCE@`/`@SHA256@` placeholders with `fill-manifest.sh`, the archive source being the local tarball, runs `flatpak-builder` into an OSTree repo and exports the bundle with `flatpak build-bundle --runtime`.
- It builds against Flatpak OBS itself (`runtime: com.obsproject.Studio`, `build-extension: true`): its libobs, obs-frontend-api and Qt 6 CMake packages and its FFmpeg headers in `/app`, and libcurl from `org.freedesktop.Sdk`, whose version must match the freedesktop runtime Flatpak OBS uses (26.08 for OBS 33). At run time the plugin uses the FFmpeg OBS ships in `/app/lib`.
- It configures with the `ubuntu-x86_64` preset plus `-DCMAKE_INSTALL_LIBDIR=lib`, installing to `/app/plugins/ReplayBufferPro/lib/obs-plugins/` and `share/obs/obs-plugins/replay-buffer-pro/`, the folders Flatpak OBS merges from every extension. Not `ubuntu-ci-x86_64`, so a new warning from a newer compiler in a future SDK doesn't fail a release.
- `tools/flatpak/run.sh` runs it in a privileged x86_64 container with the SDK and Flatpak OBS in the Docker volume `rbp-flatpak`. Under emulation on an ARM host it runs bubblewrap through `bwrap-no-seccomp`, because an ARM kernel rejects bubblewrap's x86_64 seccomp filter.
- The manifest is also ready for a Flathub submission (archive source by release URL and sha256, `x-checker-data` on `releases/latest`); `tools/flatpak/README.md` has the steps.

### Post-build rundir

After building, output is automatically copied to:
- **Windows**: `build_x64/rundir/<config>/`
- **macOS**: `build_macos/rundir/<config>/`
- **Linux**: `build_x86_64/rundir/<config>/`

### Windows resource file

The plugin DLL embeds a VERSIONINFO resource (`cmake/windows/resources/resource.rc.in`) with version, author, and copyright metadata.

## Install and packaging

### Install target

- **Windows**: `cmake --install build_x64` installs to `%ALLUSERSPROFILE%/obs-studio/plugins/<plugin-name>/bin/64bit/` and `<plugin-name>/data/`.
- **macOS**: `cmake --install build_macos` installs the `.plugin` bundle to `~/Library/Application Support/obs-studio/plugins/` and also runs `pkgbuild`/`productbuild` to produce a `.pkg` installer.
- **Linux**: `cmake --install build_x86_64` installs `replay-buffer-pro.so` to `/usr/lib/x86_64-linux-gnu/obs-plugins/` and `data/` to `/usr/share/obs/obs-plugins/replay-buffer-pro/`, where OBS from apt looks for plugins.

### prepare_release target (Windows only)

`cmake --build build_x64 --target prepare_release --config RelWithDebInfo` creates a release zip at `build_x64/releases/<version>/replay-buffer-pro-windows-x64.zip` with the standard OBS plugin directory structure. This target is Windows-only; macOS packaging is handled by the CI `package-macos` script.

### macOS packaging

macOS packaging is performed by `.github/scripts/package-macos` (CI only). It produces:
- `replay-buffer-pro-<version>-macos-universal.tar.xz` — the `.plugin` bundle archive
- `replay-buffer-pro-<version>-macos-universal.pkg` — installer package (when `--package` is passed)

Codesigning and notarization are supported via `--codesign` and `--notarize` flags using `CODESIGN_IDENT`, `CODESIGN_IDENT_INSTALLER`, and notarytool credentials.

### Linux packaging

`.github/scripts/package-ubuntu` always builds CPack's `package_source` target (`replay-buffer-pro-<version>-source.tar.xz`). Then:
- with `--package`: CPack's `package` target builds `replay-buffer-pro-<version>-x86_64-ubuntu<release>-linux-gnu.deb` and its `-dbgsym.ddeb`, where `<release>` is the host's Ubuntu release (24.04 or 26.04), read from `ID` and `VERSION_ID` in `/etc/os-release`. On any other host, Ubuntu derivatives included, the name has no release (`-x86_64-linux-gnu.deb`). `dpkg-shlibdeps` fills in the `.deb`'s dependencies from the libraries the plugin links. It only adds an unversioned `obs-studio`, which Ubuntu's own older OBS also satisfies, so `cmake/linux/defaults.cmake` adds `obs-studio (>= X.Y)` from the libobs the plugin was built against. The `libav*` sonames differ between the releases (FFmpeg 6.1 on 24.04, 8.0 on 26.04), so a `.deb` for one release cannot install on the other. Configure warns when the OBS is not in `buildspec.json`'s `dependencies.obs-studio.linuxSupported`, because `ClipWriter` mirrors that version's `ffmpeg-mux`. Tag builds use the Release config, which `cmake/linux/compilerconfig.cmake` builds with `-g` so the `-dbgsym.ddeb` holds real debug info.
- without it: `replay-buffer-pro-<version>-x86_64-ubuntu-gnu.tar.xz`, an archive of the staged `lib/` and `share/` trees.

The `.deb` installs next to OBS from apt. The Flatpak build of OBS can't load it; it gets the `.flatpak` bundle built from the source `.tar.xz` instead (see [Flatpak bundle](#flatpak-bundle)).

The source `.tar.xz` holds the whole checkout minus `.git/`, `.github/`, `build_*` and `release/`, plus the untracked folders a local working tree may hold (`build/`, `.deps/`, `site/node_modules/`, `site/dist/`, `site/.astro/`, editor folders and `.claude/settings.local.json`), and builds with nothing else: the presets, `cmake/`, `buildspec.json` and `data/` are inside, and nothing reads git at configure or build time.

### CI / GitHub Actions

- **Push to main/develop**: Triggers build for Windows, macOS, Ubuntu 24.04 and 26.04 and the Flatpak bundle, uploads artifacts.
- **Semver tag push** (e.g. `1.4.0`, `1.5.0-beta1`): Triggers build + creates a draft GitHub release with the Windows zip, macOS `.pkg`, Ubuntu 24.04 and 26.04 `.deb`s and their `-dbgsym.ddeb`s, the `.flatpak` bundle, and the source `.tar.xz` attached with checksums. The `flatpak-build` job runs `tools/flatpak/build-bundle.sh` in Flathub's build image (`ghcr.io/flathub-infra/flatpak-github-actions:freedesktop-<version>`, privileged for bubblewrap) on the source `.tar.xz` the 26.04 Ubuntu job uploaded, so the bundle is built from the same tarball the release carries.
- **Pull requests**: Triggers the same builds to validate the PR.
- **Website**: a push to `main` that touches `site/**` or `buildspec.json` builds `site/` with Astro and deploys GitHub Pages. Pull requests touching the same paths run a build-only Site Check. The site is a separate Node project (Node version in `site/.nvmrc`); it reads the plugin version from `buildspec.json` and page dates from git history at build time, so the deploy checks out full history.

Changes that only touch the website, docs or agent config skip the plugin build. `push.yaml` and
`pr-pull.yaml` list `site/**`, `.github/workflows/pages.yaml`, `.github/workflows/site-check.yaml`,
`'**.md'`, `docs/**`, `.agents/**`, `.claude/**`, `skills-lock.json` and `tools/linux-desktop/**` under `paths-ignore`, so a branch push or pull request builds
only when at least one changed file falls outside them; a change that mixes these with plugin files
builds as usual. Tag pushes always build, because GitHub does not
evaluate path filters for tags. A workflow skipped by a path filter leaves its checks `Pending`, so
making the plugin build a required status check would block pull requests that only touch these paths. Before
requiring it, replace the filter with an always-running job that inspects the changed files and
skips the build with `if:` (a job skipped by a condition reports success).

macOS CI does not use ccache: Xcode 26 injects deployment-target environment variables into
shell-wrapped compiler invocations, and a ccache launcher script triggers a `conflicting deployment
targets` error from clang (see [ccache/ccache#1670](https://github.com/ccache/ccache/discussions/1670)).
Instead, the `macos-ci` preset enables Xcode's built-in compilation cache
(`CMAKE_XCODE_ATTRIBUTE_COMPILATION_CACHE_ENABLE_CACHING` / `_CAS_PATH`), and the CI workflow caches
the CAS directory (`~/Library/Developer/Xcode/DerivedData/CompilationCache.noindex`, passed via the
`XCODE_CAS_PATH` env var) across runs instead of `.ccache`. The Ubuntu job does use ccache: the
`ubuntu-ci-x86_64` preset sets `ENABLE_CCACHE`, and the workflow caches `.ccache/` per Ubuntu release and build configuration, saving one entry per commit and restoring the newest earlier one.

Workflow files:
- `.github/workflows/push.yaml` — push and tag triggers
- `.github/workflows/pr-pull.yaml` — PR triggers
- `.github/workflows/build-project.yaml` — reusable build workflow (Windows, macOS and Ubuntu jobs)
- `.github/workflows/pages.yaml` — builds `site/` and deploys GitHub Pages from `main`
- `.github/workflows/site-check.yaml` — builds and type-checks `site/` on pull requests
- `.github/actions/build-plugin/action.yaml` — composite build action (Windows, macOS and Ubuntu)
- `.github/actions/package-plugin/action.yaml` — composite packaging action (Windows, macOS and Ubuntu)
- `.github/actions/setup-macos-codesigning/action.yaml` — keychain + certificate setup for macOS codesigning
- `.github/scripts/Build-Windows.ps1` — PowerShell build script
- `.github/scripts/Package-Windows.ps1` — PowerShell packaging script
- `.github/scripts/build-macos` — Zsh build script (configures with `macos-ci` preset, builds via xcodebuild)
- `.github/scripts/package-macos` — Zsh packaging script (tar.xz archive or .pkg installer)
- `.github/scripts/.Brewfile` — Homebrew dependencies for macOS CI runner (coreutils, cmake, jq, xcbeautify)
- `.github/scripts/build-ubuntu` — Zsh build script (installs apt dependencies, configures with the `ubuntu-ci-x86_64` preset on CI, builds with Ninja, stages into `release/<config>/`)
- `.github/scripts/package-ubuntu` — Zsh packaging script (source `.tar.xz`, then `.deb` + `-dbgsym.ddeb` or a binary `.tar.xz`)
- `.github/scripts/.Aptfile` — apt tools for the Ubuntu runner (cmake, ccache, git, jq, ninja-build, pkg-config)
- `.github/scripts/utils.zsh/` — Zsh autoload functions shared by the macOS and Ubuntu scripts (`check_ubuntu` and `setup_ubuntu` install the Ubuntu dependencies)

#### macOS CI secrets (required for codesigning)

The following GitHub repository secrets must be configured to enable codesigning and notarization on release builds:

| Secret | Description |
|---|---|
| `MACOS_SIGNING_APPLICATION_IDENTITY` | Developer ID Application identity string |
| `MACOS_SIGNING_INSTALLER_IDENTITY` | Developer ID Installer identity string |
| `MACOS_SIGNING_CERT` | PKCS12 certificate in base64 format |
| `MACOS_SIGNING_CERT_PASSWORD` | Password for the PKCS12 certificate |
| `MACOS_KEYCHAIN_PASSWORD` | Password for the temporary CI keychain |
| `MACOS_SIGNING_PROVISIONING_PROFILE` | Provisioning profile in base64 format (optional) |
| `MACOS_NOTARIZATION_USERNAME` | Apple ID for notarization |
| `MACOS_NOTARIZATION_PASSWORD` | App-specific password for notarization |

Builds without these secrets configured will use ad-hoc signing (`-`), which works locally but produces unsigned artifacts.

## Localization

- Default locale is `en-US`.
- Localization keys live in `data/locale/en-US.ini` under `[ReplayBufferPro]`.
- UI text is accessed via `obs_module_text(...)`.

## Related code

- `buildspec.json`
- `CMakePresets.json`
- `CMakeLists.txt`
- `cmake/` (all modules)
- `.github/` (CI workflows, actions, scripts)
- `data/locale/en-US.ini`

# Build, Packaging, and Localization

This document describes how the plugin is built, packaged, and localized.

## Build system (CMake)

The build system follows the [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate) structure.

- Project name and version are defined in `buildspec.json` (not directly in `CMakeLists.txt`).
- C++ standard: C++17.
- Sources: module entry, plugin, managers, UI, and utilities.
- Links against OBS (libobs, obs-frontend-api), Qt6 (Widgets/Core), and FFmpeg libraries (avformat, avcodec, avutil).

### Key files

| File | Purpose |
|---|---|
| `buildspec.json` | Plugin metadata (name, version, author) and pinned dependency versions with SHA256 hashes (Windows + macOS; Linux uses system packages) |
| `CMakePresets.json` | Configure/build presets for Windows x64, macOS universal, and Ubuntu x86_64 (local and CI) |
| `CMakeLists.txt` | Main build file — follows the template pattern |
| `cmake/common/bootstrap.cmake` | Entry point: reads `buildspec.json`, sets plugin variables, blocks in-source builds |
| `cmake/common/osconfig.cmake` | OS detection, sets `OS_WINDOWS`/`OS_MACOS`, adds platform-specific module path |
| `cmake/common/buildnumber.cmake` | Incremental build number counter |
| `cmake/common/compiler_common.cmake` | C/C++17 standard, visibility presets, clang/AppleClang warning flags |
| `cmake/common/buildspec_common.cmake` | Generic dependency downloader: fetches/extracts archives, builds OBS from source (Windows and macOS paths) |
| `cmake/common/helpers_common.cmake` | Shared helper functions |
| `cmake/common/ccache.cmake` | Optional ccache support for CI (includes ObjC/ObjC++ launchers for macOS) |
| `cmake/windows/buildspec.cmake` | Windows platform slice for dependency setup |
| `cmake/windows/compilerconfig.cmake` | MSVC-specific compiler flags (`/W3`, `/utf-8`, `/permissive-`, LTO) |
| `cmake/windows/defaults.cmake` | Sets install prefix to `%ALLUSERSPROFILE%/obs-studio/plugins` |
| `cmake/windows/helpers.cmake` | `set_target_properties_plugin()`: install rules, rundir post-build copy, `.rc` resource |
| `cmake/windows/resources/resource.rc.in` | Windows VERSIONINFO resource template embedded in the DLL |
| `cmake/macos/buildspec.cmake` | macOS platform slice: sets `arch=universal`, `platform=macos`, calls `_check_dependencies_macos()`, clears quarantine |
| `cmake/macos/compilerconfig.cmake` | Requires Xcode generator, checks SDK ≥ 15.0 / Xcode ≥ 16.0, sets dSYM release flags |
| `cmake/macos/defaults.cmake` | Sets install prefix to `~/Library/Application Support/obs-studio/plugins`, RPATH settings |
| `cmake/macos/helpers.cmake` | `set_target_properties_plugin()`: bundle properties, Xcode attributes, rundir copy, pkgbuild/productbuild packaging |
| `cmake/macos/xcode.cmake` | Full Xcode attribute configuration: ccache wrappers, codesigning, hardened runtime, dSYM, warning flags |
| `cmake/macos/resources/distribution.in` | XML installer distribution definition for `productbuild` |
| `cmake/macos/resources/create-package.cmake.in` | CMake install script that runs `pkgbuild` + `productbuild` to produce a `.pkg` |
| `cmake/macos/resources/ccache-launcher-c.in` | Shell wrapper to invoke ccache for C compilation in Xcode |
| `cmake/macos/resources/ccache-launcher-cxx.in` | Shell wrapper to invoke ccache for C++ compilation in Xcode |
| `cmake/linux/compilerconfig.cmake` | GCC/Clang-specific compiler flags (`-fopenmp-simd`, version-gated warnings) |
| `cmake/linux/defaults.cmake` | `GNUInstallDirs`, CPack DEB config, system `find_package(libobs)` fallback |
| `cmake/linux/helpers.cmake` | `set_target_properties_plugin()`: install rules, rundir copy, target resources |

### Dependency management

Dependencies are auto-downloaded at CMake configure time into `.deps/`:
- **OBS Studio source** — built from source at configure time (libobs + obs-frontend-api)
- **Prebuilt obs-deps** — pre-compiled FFmpeg, and other OBS dependencies
- **Prebuilt Qt6** — pre-compiled Qt6 for MSVC

Versions and SHA256 hashes are pinned in `buildspec.json` under `dependencies.<name>.hashes` for both `windows-x64` and `macos`. The `cmake/common/buildspec_common.cmake` module handles downloading, hash verification, extraction, and building OBS.

**Linux (Ubuntu) uses system packages instead:**
- **OBS Studio** — installed from the OBS PPA via apt (`obs-studio` package)
- **Qt6** — installed from system packages (`qt6-base-dev`, `libqt6svg6-dev`, etc.)
- **FFmpeg** — installed from system packages (`libavformat-dev`, `libavcodec-dev`, `libavutil-dev`)
- **Build tools** — `build-essential`, `cmake`, `ninja-build`, `pkg-config`, `ccache`

No dependencies are downloaded into `.deps/` on Linux; everything comes from the system package manager.

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

Requires Xcode 16+ and macOS SDK 15.0+.

```bash
# Configure (first run downloads deps and builds OBS — takes a few minutes)
cmake --preset macos

# Build (universal binary: arm64 + x86_64, deployment target macOS 12.0)
cmake --build --preset macos

# Install to ~/Library/Application Support/obs-studio/plugins/
cmake --install build_macos --config RelWithDebInfo
```

The configure step is identical in structure to Windows: downloads obs-deps, Qt6, and OBS Studio source, then builds OBS. The `cmake/macos/buildspec.cmake` sets `arch=universal` and `platform=macos`, and removes macOS quarantine attributes from downloaded archives.

The build produces a `.plugin` bundle at `build_macos/rundir/<config>/replay-buffer-pro.plugin` for quick testing.

### Linux (Ubuntu) build

Requires Ubuntu 22.04+ or compatible distro with OBS PPA access.

```bash
# Install system dependencies
sudo add-apt-repository ppa:obsproject/obs-studio
sudo apt update
sudo apt install build-essential cmake ninja-build pkg-config ccache \
  libavformat-dev libavcodec-dev libavutil-dev \
  obs-studio qt6-base-dev qt6-base-private-dev libqt6svg6-dev \
  libgles2-mesa-dev libsimde-dev

# Configure
cmake --preset ubuntu-x86_64

# Build (x86_64 binary)
cmake --build --preset ubuntu-x86_64

# Install to system paths
sudo cmake --install build_x86_64 --prefix /usr --config RelWithDebInfo
```

The configure step finds OBS and FFmpeg via `pkg-config` and system CMake modules. No dependencies are downloaded.

The build produces a `.so` shared object at `build_x86_64/rundir/<config>/replay-buffer-pro.so` for quick testing.

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
- **Linux**: `cmake --install build_x86_64 --prefix /usr` installs the `.so` to `/usr/lib/x86_64-linux-gnu/obs-plugins/` and data files to `/usr/share/obs/obs-plugins/<plugin-name>/`. CPack produces a `.deb` package.

### prepare_release target (Windows only)

`cmake --build build_x64 --target prepare_release --config RelWithDebInfo` creates a release zip at `build_x64/releases/<version>/replay-buffer-pro-windows-x64.zip` with the standard OBS plugin directory structure. This target is Windows-only; macOS and Linux packaging are handled by CI scripts (`package-macos` and `package-ubuntu`).

### macOS packaging

macOS packaging is performed by `.github/scripts/package-macos` (CI only). It produces:
- `replay-buffer-pro-<version>-macos-universal.tar.xz` — the `.plugin` bundle archive
- `replay-buffer-pro-<version>-macos-universal.pkg` — installer package (when `--package` is passed)

Codesigning and notarization are supported via `--codesign` and `--notarize` flags using `CODESIGN_IDENT`, `CODESIGN_IDENT_INSTALLER`, and notarytool credentials.

### Linux packaging

Linux packaging is performed by `.github/scripts/package-ubuntu` (CI only). It produces:
- `replay-buffer-pro-<version>-<os>-x86_64-gnu.tar.xz` — the `.so` and data files archive
- `replay-buffer-pro_<version>-<arch>.deb` — Debian package (when `--package` is passed, via CPack)

Debug symbol packages (`.ddeb`) are also produced when packaging.

### CI / GitHub Actions

- **Push to main/master**: Triggers build for Windows, macOS, and Ubuntu 24.04; uploads artifacts.
- **Semver tag push** (e.g. `1.4.0`, `1.5.0-beta1`): Triggers build + creates a draft GitHub release with Windows zip, macOS `.tar.xz`, macOS `.pkg`, and Linux `.tar.xz`/`.deb` attached with checksums.
- **Pull requests**: Triggers build for all platforms to validate the PR.

Workflow files:
- `.github/workflows/push.yaml` — push and tag triggers
- `.github/workflows/pr-pull.yaml` — PR triggers
- `.github/workflows/build-project.yaml` — reusable build workflow (Windows, macOS, and Ubuntu jobs)
- `.github/actions/build-plugin/action.yaml` — composite build action (Windows, macOS, Linux)
- `.github/actions/package-plugin/action.yaml` — composite packaging action (Windows, macOS, Linux)
- `.github/actions/setup-macos-codesigning/action.yaml` — keychain + certificate setup for macOS codesigning
- `.github/scripts/Build-Windows.ps1` — PowerShell build script
- `.github/scripts/Package-Windows.ps1` — PowerShell packaging script
- `.github/scripts/build-macos` — Zsh build script (configures with `macos-ci` preset, builds via xcodebuild)
- `.github/scripts/package-macos` — Zsh packaging script (tar.xz archive or .pkg installer)
- `.github/scripts/.Brewfile` — Homebrew dependencies for macOS CI runner (ccache, cmake, jq, xcbeautify)
- `.github/scripts/build-ubuntu` — Zsh build script for Ubuntu (configures with `ubuntu-ci-x86_64` preset)
- `.github/scripts/package-ubuntu` — Zsh packaging script for Ubuntu (tar.xz archive or .deb package)
- `.github/scripts/.Aptfile` — APT dependencies for Ubuntu CI runner (cmake, ccache, git, jq, ninja-build, pkg-config)
- `.github/scripts/utils.zsh/` — Zsh autoload functions shared by macOS and Ubuntu CI scripts

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

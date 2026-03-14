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
| `buildspec.json` | Plugin metadata (name, version, author) and pinned dependency versions with SHA256 hashes |
| `CMakePresets.json` | Configure/build presets for Windows x64 (local and CI) |
| `CMakeLists.txt` | Main build file — follows the template pattern |
| `cmake/common/bootstrap.cmake` | Entry point: reads `buildspec.json`, sets plugin variables, blocks in-source builds |
| `cmake/common/osconfig.cmake` | OS detection, sets `OS_WINDOWS`, adds platform-specific module path |
| `cmake/common/buildnumber.cmake` | Incremental build number counter |
| `cmake/common/compiler_common.cmake` | C/C++17 standard, visibility presets, clang warning flags |
| `cmake/common/buildspec_common.cmake` | Generic dependency downloader: fetches/extracts archives, builds OBS from source |
| `cmake/common/helpers_common.cmake` | Shared helper functions |
| `cmake/common/ccache.cmake` | Optional ccache support for CI |
| `cmake/windows/buildspec.cmake` | Windows platform slice for dependency setup |
| `cmake/windows/compilerconfig.cmake` | MSVC-specific compiler flags (`/W3`, `/utf-8`, `/permissive-`, LTO) |
| `cmake/windows/defaults.cmake` | Sets install prefix to `%ALLUSERSPROFILE%/obs-studio/plugins` |
| `cmake/windows/helpers.cmake` | `set_target_properties_plugin()`: install rules, rundir post-build copy, `.rc` resource |
| `cmake/windows/resources/resource.rc.in` | Windows VERSIONINFO resource template embedded in the DLL |

### Dependency management

Dependencies are auto-downloaded at CMake configure time into `.deps/`:
- **OBS Studio source** — built from source at configure time (libobs + obs-frontend-api)
- **Prebuilt obs-deps** — pre-compiled FFmpeg, and other OBS dependencies
- **Prebuilt Qt6** — pre-compiled Qt6 for MSVC

Versions and SHA256 hashes are pinned in `buildspec.json`. The `cmake/common/buildspec_common.cmake` module handles downloading, hash verification, extraction, and building OBS.

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

### Non-Windows build

Currently not fully supported. The cmake module structure is prepared for Linux and macOS (via `cmake/linux/` and `cmake/macos/` directories) but those platform modules are not yet implemented.

### Post-build rundir

After building, the DLL, PDB, and data files are automatically copied to `build_x64/rundir/<config>/` for quick testing without a full install step.

### Windows resource file

The plugin DLL embeds a VERSIONINFO resource (`cmake/windows/resources/resource.rc.in`) with version, author, and copyright metadata.

## Install and packaging

### Install target

`cmake --install build_x64` installs to `%ALLUSERSPROFILE%/obs-studio/plugins/<plugin-name>/bin/64bit/` and `<plugin-name>/data/`.

### prepare_release target

`cmake --build build_x64 --target prepare_release --config RelWithDebInfo` creates a release zip at `build_x64/releases/<version>/replay-buffer-pro-windows-x64.zip` with the standard OBS plugin directory structure.

### CI / GitHub Actions

- **Push to main/master**: Triggers build and uploads artifacts.
- **Semver tag push** (e.g. `1.4.0`, `1.5.0-beta1`): Triggers build + creates a draft GitHub release with the zip attached and checksums.
- **Pull requests**: Triggers build to validate the PR.

Workflow files:
- `.github/workflows/push.yaml` — push and tag triggers
- `.github/workflows/pr-pull.yaml` — PR triggers
- `.github/workflows/build-project.yaml` — reusable build workflow
- `.github/actions/build-plugin/action.yaml` — composite build action
- `.github/actions/package-plugin/action.yaml` — composite packaging action
- `.github/scripts/Build-Windows.ps1` — PowerShell build script
- `.github/scripts/Package-Windows.ps1` — PowerShell packaging script

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

# Build, Packaging, and Localization

This document describes how the plugin is built, packaged, and localized.

## Build system (CMake)
- Project name: `replay-buffer-pro`.
- C++ standard: C++17.
- Sources: module entry, plugin, managers, UI, and utilities.
- Links against OBS, Qt6 (Widgets/Core), and FFmpeg libraries.

### Windows build
- Assumes an OBS Studio checkout adjacent to this repo (`../obs-studio`).
- Locates FFmpeg libraries from OBS deps (`.deps/obs-deps-*`).
- Links against `obs.lib`, `obs-frontend-api.lib`, `avformat.lib`, `avcodec.lib`, `avutil.lib`.

### Linux build
- Uses `find_package(libobs REQUIRED)` and `find_package(obs-frontend-api REQUIRED)` to locate OBS via its CMake package config (requires OBS built from source with `cmake --install`, or a distro package that ships CMake configs).
- Uses `pkg_check_modules` for `libavformat`, `libavcodec`, `libavutil` (system FFmpeg).
- Links against `OBS::libobs` and `OBS::obs-frontend-api` CMake targets.
- Default install prefix: `${CMAKE_INSTALL_PREFIX}/lib/obs-plugins` (plugin) and `${CMAKE_INSTALL_PREFIX}/share/obs/obs-plugins/<name>` (data).

### macOS build
- Similar to Windows - requires OBS Studio built from source.
- Requires OBS Studio source at `../obs-studio` (clone from https://github.com/obsproject/obs-studio).
- Looks for OBS build in `build_macos/` or `build/` directory.
- Uses OBS source headers from `../obs-studio/libobs` and `../obs-studio/frontend/api`.
- Uses `obsconfig.h` from OBS build directory (`${OBS_BUILD_DIR}/config/`).
- Requires `simde` headers from Homebrew (`brew install simde`).
- Uses `pkg_check_modules` for FFmpeg from Homebrew (`brew install ffmpeg`).
- Links against OBS build libraries: `libobs.dylib` and `libobs-frontend-api.dylib`.
- Default install prefix: `${CMAKE_INSTALL_PREFIX}/obs-plugins` (plugin) and `${CMAKE_INSTALL_PREFIX}/data/obs-plugins/<name>` (data).
- For a user install, set `CMAKE_INSTALL_PREFIX` to `~/Library/Application Support/obs-studio/plugins/replay-buffer-pro`.
- Apple Silicon (`arm64`) and Intel (`x86_64`) are both supported; the target architecture is determined by the CMake toolchain.

#### macOS Build Steps

```bash
# 1. Install dependencies
brew install pkg-config ffmpeg simde cmake qt6

# 2. Clone OBS Studio source
cd ..
git clone --depth 1 https://github.com/obsproject/obs-studio.git

# 3. Build OBS Studio
cd obs-studio
cmake -B build_macos -S . -DCMAKE_PREFIX_PATH="/opt/homebrew"
cmake --build build_macos

# 4. Build the plugin
cd ../replay-buffer-pro
mkdir -p build && cd build
cmake ..
cmake --build .

# 5. Install to OBS plugins folder
cmake --install . --prefix "$HOME/Library/Application Support/obs-studio/plugins/replay-buffer-pro"
```

## Install and packaging
- `cmake --install` installs the plugin binary and data files on all platforms.
- Install destinations are relative to `CMAKE_INSTALL_PREFIX` (not hardcoded absolute paths).
- On Windows, `CMAKE_INSTALL_PREFIX` defaults to `C:/Program Files/obs-studio` if not set.
- `prepare_release` builds a platform-specific release directory structure and zips it.
  - Output zip is named `replay-buffer-pro-<platform>.zip` where `<platform>` is one of `windows-x64`, `linux-x86_64`, `macos-x86_64`, or `macos-arm64`.

## Localization
- Default locale is `en-US`.
- Localization keys live in `data/locale/en-US.ini` under `[ReplayBufferPro]`.
- UI text is accessed via `obs_module_text(...)`.

## Related code
- `CMakeLists.txt`
- `data/locale/en-US.ini`

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

### Non-Windows build
- Uses `find_package(LibObs REQUIRED)`.
- Uses `pkg_check_modules` for libavformat/libavcodec/libavutil.

## Install and packaging
- Installs plugin binary into OBS plugin directory and data files into OBS data path.
- `prepare_release` builds a release directory structure and zips it.

## Localization
- Default locale is `en-US`.
- Localization keys live in `data/locale/en-US.ini` under `[ReplayBufferPro]`.
- UI text is accessed via `obs_module_text(...)`.

## Related code
- `CMakeLists.txt`
- `data/locale/en-US.ini`

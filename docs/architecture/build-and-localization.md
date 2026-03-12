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

#### Release availability
- **Prebuilt binaries**: Available for both Apple Silicon (`arm64`) and Intel (`x86_64`) architectures
- Release packages: `replay-buffer-pro-macos-arm64.zip` and `replay-buffer-pro-macos-x86_64.zip`

#### Building from source
- Requires OBS Studio built from source with matching architecture.
- **Requires full Xcode application** (not just Command Line Tools) - OBS requires Xcode generator.
- Requires OBS Studio source at `../obs-studio` (clone from https://github.com/obsproject/obs-studio) or specify via `OBS_SOURCE_DIR` CMake option.
- Looks for OBS build in `build_macos/` or `build/` directory, supporting multiple Xcode configs (`Debug`, `Release`, `RelWithDebInfo`).
- Uses OBS source headers from `../obs-studio/libobs` and `../obs-studio/frontend/api`.
- Uses `obsconfig.h` from OBS build directory (`${OBS_BUILD_DIR}/config/`).
- Requires `simde` headers from Homebrew (`brew install simde`).
- Uses `pkg_check_modules` for FFmpeg from Homebrew (`brew install ffmpeg`).
- **Automatic Homebrew detection**: CMake detects Homebrew prefix automatically:
  - Apple Silicon: `/opt/homebrew`
  - Intel: `/usr/local`
  - Can be overridden with `HOMEBREW_PREFIX` environment variable
- Links against OBS build libraries: `libobs.framework/libobs` or `libobs.dylib` and `obs-frontend-api.dylib`.
- Default install prefix: `${CMAKE_INSTALL_PREFIX}/obs-plugins` (plugin) and `${CMAKE_INSTALL_PREFIX}/data/obs-plugins/<name>` (data).
- For a user install: `cmake --install . --prefix "$HOME/Library/Application Support/obs-studio/plugins/replay-buffer-pro"`
- Apple Silicon (`arm64`) and Intel (`x86_64`) are both supported; the target architecture is determined by CMake toolchain or `CMAKE_OSX_ARCHITECTURES`.

#### macOS Build Steps

```bash
# 1. Install full Xcode from App Store, then:
sudo xcode-select --switch /Applications/Xcode.app/Contents/Developer
sudo xcodebuild -license accept
brew install pkg-config ffmpeg simde cmake qt6

# 2. Clone OBS Studio source
cd ..
git clone --recursive --depth 1 https://github.com/obsproject/obs-studio.git

# 3. Build OBS Studio (requires Xcode generator)
cd obs-studio

# For Apple Silicon:
cmake -B build_macos -S . -G Xcode \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt6)" \
  -DCMAKE_OSX_ARCHITECTURES="arm64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="11.0"

# For Intel:
cmake -B build_macos -S . -G Xcode \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt6)" \
  -DCMAKE_OSX_ARCHITECTURES="x86_64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="11.0"

cmake --build build_macos

# 4. Build the plugin
cd ../replay-buffer-pro

# Configure (automatically detects OBS and Homebrew)
# For Apple Silicon:
cmake -B build -G Xcode \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt6)" \
  -DCMAKE_OSX_ARCHITECTURES="arm64"

# For Intel:
cmake -B build -G Xcode \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt6)" \
  -DCMAKE_OSX_ARCHITECTURES="x86_64"

# Build
cmake --build build

# 5. Install to OBS plugins folder
cmake --install build --prefix "$HOME/Library/Application Support/obs-studio/plugins/replay-buffer-pro"
```

**Important:** OBS Studio on macOS requires the **Xcode generator**, which only works with the full Xcode application from the App Store or Apple Developer. Command Line Tools alone are not sufficient.

**Note:** The CMake configuration automatically:
- Detects your Homebrew installation (`/opt/homebrew` for Apple Silicon, `/usr/local` for Intel)
- Finds OBS Studio source at `../obs-studio` (or use `-DOBS_SOURCE_DIR=/path/to/obs`)
- Discovers OBS build artifacts across Debug/Release/RelWithDebInfo configurations
- Configures FFmpeg via pkg-config from Homebrew

## Install and packaging
- `cmake --install` installs the plugin binary and data files on all platforms.
- Install destinations are relative to `CMAKE_INSTALL_PREFIX` (not hardcoded absolute paths).
- On Windows, `CMAKE_INSTALL_PREFIX` defaults to `C:/Program Files/obs-studio` if not set.
- `prepare_release` builds a platform-specific release directory structure and zips it.
  - Output zip is named `replay-buffer-pro-<platform>.zip` where `<platform>` is one of:
    - `windows-x64` - Windows 10/11 64-bit
    - `linux-x86_64` - Linux 64-bit
    - `macos-arm64` - macOS Apple Silicon (M1/M2/M3/M4)
    - `macos-x86_64` - macOS Intel

### macOS Release Installation

1. Download the correct architecture:
   - **Apple Silicon (M1/M2/M3/M4)**: `replay-buffer-pro-macos-arm64.zip`
   - **Intel**: `replay-buffer-pro-macos-x86_64.zip`

2. Extract and copy to OBS:
   ```bash
   # Copy to system OBS application
   cp -r obs-studio/* "/Applications/OBS.app/Contents/Resources/"
   
   # Or to user plugins directory
   mkdir -p "$HOME/Library/Application Support/obs-studio/plugins"
   cp -r obs-studio/* "$HOME/Library/Application Support/obs-studio/plugins/"
   ```

## Localization
- Default locale is `en-US`.
- Localization keys live in `data/locale/en-US.ini` under `[ReplayBufferPro]`.
- UI text is accessed via `obs_module_text(...)`.

## Continuous Integration

### GitHub Actions (macOS)
- Automated builds for both Apple Silicon (`macos-14` runner) and Intel (`macos-13` runner)
- Builds OBS Studio from source, then builds the plugin
- Creates release packages automatically on tagged releases
- Workflow file: `.github/workflows/macos.yml`

## Related code
- `CMakeLists.txt`
- `data/locale/en-US.ini`
- `.github/workflows/macos.yml`

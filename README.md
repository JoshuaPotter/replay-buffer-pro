# Replay Buffer Pro

[![GitHub Release](https://img.shields.io/github/v/release/joshuapotter/replay-buffer-pro)
![GitHub Release Date](https://img.shields.io/github/release-date/joshuapotter/replay-buffer-pro?display_date=published_at)](https://github.com/JoshuaPotter/replay-buffer-pro/releases/latest)

This OBS Studio plugin expands upon the built-in Replay Buffer, allowing users to save recent footage at different lengths with customizable save buttons, similar to how PlayStation/Xbox's "Save Recent Gameplay" functionality.

**Note:** This plugin is 64-bit only, as it requires OBS Studio 29.0.0+ which dropped 32-bit support.

## How It Works
OBS keeps a rolling buffer of the last few seconds or minutes of footage in memory using the built-in replay buffer. The length of this footage is defined in settings. If the amount of footage exceeds the length in settings, old footage is overwritten as new footage is recorded.

Unlike the default Replay Buffer, which saves a fixed duration, this OBS Studio plugin allows users to save different lengths on demand. Set the replay buffer length, then clip custom lengths of footage automatically. Example: Set your replay buffer to 10 minutes. Save the last 30 seconds, 2 minutes, or 5 minutes instantly with UI buttons or hotkeys.

The project website is currently hosted via GitHub Pages.

![Screenshot](./screenshot.png)

## Usage

### Saving Clips
1. Start the Replay Buffer in OBS
2. Click any save clip button (customizable durations) or use the assigned hotkey
3. Use the Customize button to set your preferred clip lengths
4. The plugin will:
   - Save the full replay buffer
   - Automatically trim to the selected duration (Without re-encoding)
   - Replace the original file with the trimmed version

### Buffer Length
- Quickly adjust built-in replay buffer length (1s to 6h) without digging through the settings

### Hotkeys
- Assign hotkeys to each save duration button in OBS Settings > Hotkeys

## Installation

### From Release

1. Download latest release
2. Extract and merge the folder `obs-studio` with your OBS Studio installation

Final file structure should look like this:
```
obs-studio/
├── obs-plugins/
│   └── 64bit/
│       └── replay-buffer-pro.dll
└── data/
    └── obs-plugins/
        └── replay-buffer-pro/
            └── locale/
                └── en-US.ini
```

### From Source

See the [Building from Source](#building-from-source) section below. After building, run `cmake --install build` to install directly into OBS, or build the `prepare_release` target to create a zip package.

## Building from Source

### Requirements (all platforms)

- OBS Studio 30.0.0+ (64-bit), built from source or installed with CMake package config
- Qt6 (Widgets + Core)
- FFmpeg (libavformat, libavcodec, libavutil)
- CMake 3.16+

---

### Windows

**Additional requirements:**
- Windows 10/11 64-bit
- Visual Studio 2022+ with "Desktop development with C++"

#### 1. Prerequisites

1. Install Visual Studio 2022+ with "Desktop development with C++"
2. Install Qt6 (MSVC 2022 64-bit) from https://www.qt.io/download-qt-installer
3. Install CMake 3.16+ from https://cmake.org/download/

#### 2. Build OBS Studio

```bash
# Clone OBS (with submodules) as a sibling of this repo
git clone --recursive https://github.com/obsproject/obs-studio.git
cd obs-studio
mkdir -p build && cd build

# Configure (obs-deps provides FFmpeg; set -DDepsPath if downloaded separately)
cmake -G "Visual Studio 17 2022" -A x64 -DDepsPath="C:/Dev/obs-deps-2022" ..

# Build OBS (RelWithDebInfo to match plugin build)
cmake --build . --config RelWithDebInfo
```

#### 3. Build Plugin

```bash
git clone https://github.com/joshuapotter/replay-buffer-pro.git
cd replay-buffer-pro
mkdir -p build && cd build

# Replace path with your local Qt 6 msvc2022_64 installation
cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.8.2/msvc2022_64" ..

cmake --build . --config RelWithDebInfo

# Install to OBS (run as Administrator with OBS closed)
cmake --install . --config RelWithDebInfo
```

**Note:** Ensure the `obs-studio` repository is located as a sibling of this plugin (same parent directory).

---

### Linux

**Additional requirements:**
- GCC 11+ or Clang 14+
- OBS Studio 30.0.0+ installed with CMake package config (e.g. built from source with `cmake --install`)
- Qt6, FFmpeg development headers: `libqt6-dev libavformat-dev libavcodec-dev libavutil-dev`

#### 1. Install dependencies (Ubuntu/Debian)

```bash
sudo apt install cmake gcc g++ \
  qt6-base-dev libavformat-dev libavcodec-dev libavutil-dev
```

#### 2. Build OBS Studio (if not already installed with CMake config)

```bash
git clone --recursive https://github.com/obsproject/obs-studio.git
cd obs-studio
cmake -B build -DENABLE_UI=OFF -DENABLE_PLUGINS=OFF
cmake --build build
sudo cmake --install build   # installs libobs and obs-frontend-api CMake config
```

#### 3. Build Plugin

```bash
git clone https://github.com/joshuapotter/replay-buffer-pro.git
cd replay-buffer-pro
cmake -B build
cmake --build build

# Install to system OBS plugin directory
sudo cmake --install build
```

To install to a custom prefix (e.g. your home directory):
```bash
cmake -B build -DCMAKE_INSTALL_PREFIX="$HOME/.config/obs-studio"
cmake --build build
cmake --install build
```

---

### macOS

**Additional requirements:**
- Xcode Command Line Tools or full Xcode
- OBS Studio 30.0.0+ installed with CMake package config (built from source or via Homebrew with cmake config)
- Qt6 and FFmpeg via Homebrew: `brew install qt6 ffmpeg`

#### 1. Install dependencies

```bash
xcode-select --install
brew install cmake qt6 ffmpeg
```

#### 2. Build OBS Studio

```bash
git clone --recursive https://github.com/obsproject/obs-studio.git
cd obs-studio
cmake -B build -DENABLE_UI=OFF -DENABLE_PLUGINS=OFF \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt6)"
cmake --build build
sudo cmake --install build
```

#### 3. Build Plugin

```bash
git clone https://github.com/joshuapotter/replay-buffer-pro.git
cd replay-buffer-pro
cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt6)"
cmake --build build

# Install to user OBS plugin directory
cmake -B build \
  -DCMAKE_INSTALL_PREFIX="$HOME/Library/Application Support/obs-studio/plugins/replay-buffer-pro" \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt6)"
cmake --install build
```

---

### Release Packaging (all platforms)

Iterate the version in `CMakeLists.txt`, then run:
```bash
cmake --build build --target prepare_release
```

The output zip will be named `replay-buffer-pro-<platform>.zip` (e.g. `windows-x64`, `linux-x86_64`, `macos-arm64`) under `build/releases/<version>/`.

### 3. Build Plugin

```bash
git clone https://github.com/joshuapotter/replay-buffer-pro.git
cd replay-buffer-pro
mkdir -p build && cd build

# Replace path with your local Qt 6 msvc2022_64
cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.8.2/msvc2022_64" ..

# Build the plugin (RelWithDebInfo to match OBS)
cmake --build . --config RelWithDebInfo

# Install (run an elevated shell and ensure OBS is closed)
cmake --install . --config RelWithDebInfo
```
**Note:** Replace the Qt path with your installation. Ensure the `obs-studio` repository is located as a sibling of this plugin (same parent directory). You may need to run the install command elevated (Run as administrator) to install to Program Files.

### 4. Release Plugin
Iterate the version in `CMakeLists.txt`, then run:
```bash
cmake -S .. -B .
cmake --build . --config RelWithDebInfo --target prepare_release
```

### Project Structure

```
obs-studio/              # OBS Studio source code (from step 2)
replay-buffer-pro/
├── CMakeLists.txt       # Build configuration
├── data/               
│   └── locale/          # Translations
├── src/                 # Source files
│   ├── managers/        # Core functionality managers
│   ├── plugin/          # Main plugin implementation
│   ├── ui/              # User interface components
│   └── utils/           # Utility classes (including video-trimmer)
├── pages/               # Project website source
└── README.md
```

## Troubleshooting

- Verify plugin DLL location
- Check OBS logs for errors
- For trimming issues:
  - Check disk space
  - Check write permissions in output directory
- When building from source
  - Ensure Qt6 and OBS paths are correct in CMake
  - If building OBS on Windows, ensure `-DDepsPath` points to your obs-deps directory
  - Run install command in a terminal with admin privileges

## Third-Party Software

This plugin uses OBS Studio's built-in FFmpeg libraries (libavformat) for video trimming functionality. FFmpeg is licensed under the LGPL v2.1+ license.

## License

GPL v2 or later. See LICENSE file for details. 

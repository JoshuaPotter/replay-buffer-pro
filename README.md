# Replay Buffer Pro

[![GitHub Release](https://img.shields.io/github/v/release/joshuapotter/replay-buffer-pro)
![GitHub Release Date](https://img.shields.io/github/release-date/joshuapotter/replay-buffer-pro?display_date=published_at)](https://github.com/JoshuaPotter/replay-buffer-pro/releases/latest)

This OBS Studio plugin expands upon the built-in Replay Buffer, allowing users to save recent footage at different lengths with customizable save buttons, similar to how PlayStation/Xbox's "Save Recent Gameplay" functionality.

**Note:** This plugin is 64-bit only, as it requires OBS Studio 29.0.0+ which dropped 32-bit support.

## How It Works
OBS keeps a rolling buffer of the last few seconds or minutes of footage in memory using the built-in replay buffer. The length of this footage is defined in settings. If the amount of footage exceeds the length in settings, old footage is overwritten as new footage is recorded.

Unlike the default Replay Buffer, which saves a fixed duration, this OBS Studio plugin allows users to save different lengths on demand. Set the replay buffer length, then clip custom lengths of footage automatically. Example: Set your replay buffer to 10 minutes. Save the last 30 seconds, 2 minutes, or 5 minutes instantly with UI buttons or hotkeys.

The project website is currently hosted via GitHub Pages.

![Plugin UI](./plugin_ui.png)

## Usage

### Saving Clips
1. Start the Replay Buffer in OBS
2. Click any save clip button (customizable durations) or use the assigned hotkey
3. Use the Customize button to set your preferred clip lengths

   ![Customize UI](./customize_ui.png)

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

#### Windows

1. Download the latest release for Windows (`replay-buffer-pro-windows-x64.zip`)
2. Extract and merge the folder `obs-studio` with your OBS Studio installation (typically `C:\Program Files\obs-studio\`)

Final file structure:
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

#### macOS

**For Intel Macs:** Download `replay-buffer-pro-macos-x86_64.zip`  
**For Apple Silicon Macs (M1/M2/M3/M4):** Download `replay-buffer-pro-macos-arm64.zip`

1. Close OBS Studio completely
2. Extract the downloaded ZIP file
3. Copy the plugin files to your OBS plugins directory:

**For Intel Macs:**
```bash
# Copy to system OBS (if installed via installer)
cp -r obs-studio/* "/Applications/OBS.app/Contents/Resources/"

# Or copy to user plugins directory
mkdir -p "$HOME/Library/Application Support/obs-studio/plugins"
cp -r obs-studio/* "$HOME/Library/Application Support/obs-studio/plugins/"
```

**For Apple Silicon Macs:**
```bash
# Copy to system OBS (if installed via installer)
cp -r obs-studio/* "/Applications/OBS.app/Contents/Resources/"

# Or copy to user plugins directory
mkdir -p "$HOME/Library/Application Support/obs-studio/plugins"
cp -r obs-studio/* "$HOME/Library/Application Support/obs-studio/plugins/"
```

**Note:** The plugin files must be placed in the correct architecture-specific location. macOS will show an error if you try to use the wrong architecture.

#### Linux

See the [Building from Source](#building-from-source) section below. Prebuilt binaries are not provided for Linux due to distribution differences.

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
- macOS 11+ with Apple Silicon (M1/M2/M3/M4) or Intel
- **Full Xcode app** (not just Command Line Tools) - download from App Store or Apple Developer
- OBS Studio 30.0.0+ 
- Homebrew dependencies: `brew install qt6 ffmpeg simde pkg-config cmake`
- OBS Studio built from source at `../obs-studio`

#### 1. Install Xcode and Homebrew dependencies

```bash
# Install full Xcode from App Store first, then:
sudo xcode-select --switch /Applications/Xcode.app/Contents/Developer
sudo xcodebuild -license accept

# Install Homebrew if not already installed
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install cmake qt6 ffmpeg simde pkg-config
```

**Note:** The plugin build system automatically detects your Homebrew prefix (`/opt/homebrew` for Apple Silicon, `/usr/local` for Intel). You can override this with the `HOMEBREW_PREFIX` environment variable.

#### 2. Clone and Build OBS Studio

```bash
# From your project parent directory (same level as this plugin)
git clone --recursive --depth 1 https://github.com/obsproject/obs-studio.git
cd obs-studio

# Configure OBS build
# For Apple Silicon (M1/M2/M3/M4):
cmake -B build_macos -S . -G Xcode \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt6)" \
  -DCMAKE_OSX_ARCHITECTURES="arm64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="11.0"

# For Intel Macs:
cmake -B build_macos -S . -G Xcode \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt6)" \
  -DCMAKE_OSX_ARCHITECTURES="x86_64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="11.0"

# Build OBS (this may take 15-30 minutes)
cmake --build build_macos
```

**Note:** OBS requires the **Xcode generator** on macOS, which requires the full Xcode application. Command Line Tools alone are not sufficient.

#### 3. Build the Plugin

```bash
# Go back to the plugin directory
cd ../replay-buffer-pro

# Configure the plugin (automatically detects OBS and Homebrew)
# For Apple Silicon:
cmake -B build -G Xcode \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt6)" \
  -DCMAKE_OSX_ARCHITECTURES="arm64"

# For Intel Macs:
cmake -B build -G Xcode \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt6)" \
  -DCMAKE_OSX_ARCHITECTURES="x86_64"

# Build the plugin
cmake --build build

# Install to OBS plugins folder
cmake --install build --prefix "$HOME/Library/Application Support/obs-studio/plugins/replay-buffer-pro"
```

#### 4. Create Release Package (Optional)

To create a distributable ZIP file:

```bash
cmake --build build --target prepare_release
```

The release package will be created at `build/releases/<version>/replay-buffer-pro-macos-<arch>.zip`.

---

### Release Packaging (all platforms)

1. Update the version in `CMakeLists.txt`
2. Reconfigure and build the `prepare_release` target:

```bash
cmake -B build   # re-run configure to pick up the new version
cmake --build build --target prepare_release
```

The output zip will be placed at `build/releases/<version>/replay-buffer-pro-<platform>.zip`:

- `windows-x64` - Windows 10/11 64-bit
- `linux-x86_64` - Linux 64-bit
- `macos-arm64` - macOS Apple Silicon (M1/M2/M3/M4)
- `macos-x86_64` - macOS Intel

## Project Structure

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

### Plugin Not Loading

**Windows:**
- Verify `replay-buffer-pro.dll` is in `obs-studio/obs-plugins/64bit/`
- Check OBS logs (`Help → Log Files → View Current Log`) for load errors

**macOS:**
- Verify the correct architecture: Apple Silicon Macs need `arm64` build, Intel Macs need `x86_64`
- Check plugin location: `~/Library/Application Support/obs-studio/plugins/replay-buffer-pro/obs-plugins/replay-buffer-pro.so`
- If you see "cannot be opened because the developer cannot be verified", you may need to allow it in System Settings → Privacy & Security

**Linux:**
- Verify `replay-buffer-pro.so` is in the correct OBS plugins directory (varies by distribution)
- Check OBS logs for linker errors

### Build Issues

**macOS specific:**
- "OBS Studio source not found": Ensure OBS is cloned as a sibling directory (`../obs-studio`)
- "obsconfig.h not found": OBS build may have failed; rebuild OBS with `cmake --build build_macos`
- "Could not find Homebrew": Install Homebrew from https://brew.sh/
- Xcode errors: Ensure full Xcode is installed (not just Command Line Tools):
  ```bash
  sudo xcode-select --switch /Applications/Xcode.app/Contents/Developer
  sudo xcodebuild -license accept
  ```

**Windows specific:**
- Ensure `-DDepsPath` points to your obs-deps directory when building OBS
- Run install command in a terminal with admin privileges

**All platforms:**
- Ensure Qt6 and OBS paths are correct in CMake
- Check that FFmpeg libraries are available (via pkg-config on macOS/Linux)

### Trimming Issues

- Check disk space availability
- Verify write permissions in the output directory
- Check OBS logs for FFmpeg/libavformat errors

## Third-Party Software

This plugin uses OBS Studio's built-in FFmpeg libraries (libavformat) for video trimming functionality. FFmpeg is licensed under the LGPL v2.1+ license.

## License

GPL v2 or later. See LICENSE file for details. 

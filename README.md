# Replay Buffer Pro

This OBS Studio plugin builds upon the built-in Replay Buffer by allowing users to save recent footage at different lengths, similar to how PlayStation/Xbox let you "Save Recent Gameplay".

**Note:** This plugin is 64-bit only, as it requires OBS Studio 29.0.0+ which dropped 32-bit support. The plugin uses Qt6 which also only provides 64-bit builds for Windows.

## How It Works
OBS keeps a rolling buffer of the last few seconds or minutes (length defined in settings) of footage in memory using the built-in replay buffer. If not manually saved, old footage is overwritten as new footage is recorded.

Unlike the default Replay Buffer, which saves a fixed duration, this OBS Studio plugin allows users to save different lengths on demand. Example: Save the last 30 seconds, 2 minutes, or 5 minutes instantly with UI buttons or hotkeys.

![Screenshot](./screenshot.png)

## Usage

### Saving Clips
1. Start the Replay Buffer in OBS
2. Click any save clip button (15s, 30s, 1m, etc.) or use the assigned hotkey
3. The plugin will:
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
├── data/
│   └── obs-plugins/
│       └── replay-buffer-pro/
│           └── locale/
│               └── en-US.ini
│           └── ffmpeg.exe
```

### From Source
- Plugin installs automatically to OBS directory when using `cmake --install . --config Release`

#### Manual Installation After Building
After building, you can manually copy the files from your build directory:
1. Copy from `build/Release/` or `build/RelWithDebInfo/`:
   - `replay-buffer-pro.dll` → `C:/Program Files/obs-studio/obs-plugins/64bit/`
2. Copy from source `data` directory:
   - Data files → `C:/Program Files/obs-studio/data/obs-plugins/replay-buffer-pro/`
3. Copy from source `deps/ffmpeg`:
   - FFmpeg files → `C:/Program Files/obs-studio/data/obs-plugins/replay-buffer-pro/`

Note: You may need administrator privileges to copy files to Program Files.


## Building from Source

### Requirements

- OBS Studio 29.0.0+ (64-bit)
- Windows 10/11 64-bit (Linux and MacOS are not supported at this time, PRs welcome!)
- Qt6 (64-bit)
- Visual Studio 2022+ with C++
- CMake 3.16+
- FFmpeg (bundled with plugin)

### 1. Prerequisites

1. Install Visual Studio 2019+ with "Desktop development with C++"
2. Install Qt6 (MSVC 2019/2022 64-bit) from https://www.qt.io/download-qt-installer
3. Install CMake 3.16+ from https://cmake.org/download/
4. Download FFmpeg:
   - Get latest build from https://github.com/BtbN/FFmpeg-Builds/releases
   - Download "ffmpeg-master-latest-win64-gpl"
   - Extract ffmpeg.exe to `deps/ffmpeg/` in the plugin directory

### 2. Build OBS Studio

```bash
git clone --recursive https://github.com/obsproject/obs-studio.git
cd obs-studio
mkdir build && cd build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config RelWithDebInfo
```

### 3. Build Plugin

```bash
git clone https://github.com/yourusername/replay-buffer-pro.git
cd replay-buffer-pro
mkdir build && cd build
cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64" ..
cmake --build . --config Release
cmake --install . --config Release
```
**Note:** Replace `C:/Qt/6.x.x/msvc2022_64` with your actual Qt installation path, example: `cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.8.2/msvc2022_64" ..`. OBS Studio should be in the same parent directory as the plugin.
**Note:** You may need to run the install command in a terminal with elevated permissions (Run as administrator) to install the plugin to the OBS Studio directory.

### 4. Release Plugin
After building the plugin, run:
```bash
cmake --build . --target create_release
```

### Project Structure

```
obs-studio/              # OBS Studio source code (from step 2)
replay-buffer-pro/
├── CMakeLists.txt       # Build configuration
├── data/               
│   └── locale/          # Translations
├── deps/
│   └── ffmpeg/          # FFmpeg executable
├── src/                 # Source files
└── README.md
```

## Troubleshooting

- Verify plugin DLL location
- Check OBS logs for errors
- For trimming issues:
  - Check disk space
  - Verify FFmpeg.exe is in the correct location
  - Check write permissions in output directory
- When building from source
  - Ensure Qt6 and OBS paths are correct in CMake
  - Run install command in a terminal with admin privileges

## Third-Party Software

This plugin includes FFmpeg (https://ffmpeg.org/) for video trimming functionality.
FFmpeg is licensed under the LGPL v2.1+ license. See FFmpeg documentation for details.

## License

GPL v2 or later. See LICENSE file for details. 

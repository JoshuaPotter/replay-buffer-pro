# Replay Buffer Pro

An OBS Studio plugin that extends the replay buffer functionality by adding the ability to quickly save segments of the replay buffer. This plugin is inspired by the background recording features of Playstation/Xbox and PC applications like Nvidia Shadowplay.

**Note:** This plugin is 64-bit only, as it requires OBS Studio 29.0.0+ which dropped 32-bit support. The plugin uses Qt6 which also only provides 64-bit builds for Windows.

## Features

- Adjust replay buffer length via slider (10s to 6h)
- Save segments of the replay buffer (15s to 30m)
- Dockable interface

![Screenshot](./screenshot.png)

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

## Installation

### From Release
1. Download latest release
2. Extract to OBS Studio installation:
   - Windows:
     - `replay-buffer-pro.dll` → `C:/Program Files/obs-studio/obs-plugins/64bit/`
     - Data files → `C:/Program Files/obs-studio/data/obs-plugins/replay-buffer-pro/`
     - FFmpeg files → `C:/Program Files/obs-studio/data/obs-plugins/replay-buffer-pro/`

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

## Usage

### Saving Segments
1. Start the Replay Buffer in OBS
2. Click any segment button (15s, 30s, 1m, etc.)
3. The plugin will:
   - Save the full replay buffer
   - Automatically trim to the selected duration
   - Replace the original file with the trimmed version

### Buffer Length
- Use the slider to adjust buffer length (10s to 6h)
- Changes take effect after restarting the replay buffer

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
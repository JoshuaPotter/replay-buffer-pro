# Replay Buffer Pro

An OBS Studio plugin that enhances the replay buffer functionality by allowing quick segment saves and easy buffer length adjustments. Inspired by the background recording features of Playstation/Xbox and PC applications like Shadowplay.

**Note:** This plugin is 64-bit only, as it requires OBS Studio 29.0.0+ which dropped 32-bit support. The plugin uses Qt6 which also only provides 64-bit builds for Windows.

## Features

- Adjust replay buffer length via slider (10s to 1h)
- Save segments of the replay buffer (15s to 30m)
- Dockable interface

## Building from Source

### Requirements

- OBS Studio 29.0.0+ (64-bit)
- Windows 10/11 64-bit
- Qt6 (64-bit)
- Visual Studio 2022+ with C++
- CMake 3.16+

### 1. Prerequisites

1. Install Visual Studio 2019+ with "Desktop development with C++"
2. Install Qt6 (MSVC 2019/2022 64-bit) from https://www.qt.io/download-qt-installer
3. Install CMake 3.16+ from https://cmake.org/download/

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
cmake -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64" ^
    -DOBS_SOURCE_DIR="path/to/obs-studio" ..
cmake --build . --config Release
cmake --install . --config Release
```
**Note:** Replace `C:/Qt/6.x.x/msvc2022_64` with your actual Qt installation path, example: `cmake -G "Visual Studio 17 2022" -A x64 ^ -DCMAKE_PREFIX_PATH="C:/Qt/6.8.2/msvc2022_64" ^ -DOBS_SOURCE_DIR="../obs-studio" ..`, where `../obs-studio` is the path to the OBS Studio source code (relative to the `replay-buffer-pro` root directory).

### Project Structure

```
replay-buffer-pro/
├── CMakeLists.txt       # Build configuration
├── data/               
│   └── locale/          # Translations
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

### From Source
- Plugin installs automatically to OBS directory when using `cmake --install . --config Release`

## Troubleshooting

- Verify plugin DLL location
- Check OBS logs for errors
- Ensure Qt6 and OBS paths are correct in CMake
- Run install with admin privileges

## License

GPL v2 or later. See LICENSE file for details. 
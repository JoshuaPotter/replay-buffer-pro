# AGENTS

This file is a concise handoff for agents working in the Replay Buffer Pro OBS plugin.

> **Current branch:** `chore/cmake-linux` — implements Linux (Ubuntu) build infrastructure. Target merge into `develop` when complete.

## Project summary
- Adds a dockable OBS UI for replay buffer controls.
- Lets users adjust replay buffer length and save clips of customizable durations.
- Trims saved replays to the last N seconds using FFmpeg libavformat (no re-encode).

## Architecture map (start here)
- Module entry + OBS integration: `src/main.cpp`
- Dock widget + UI orchestration: `src/plugin/plugin.hpp`, `src/plugin/plugin.cpp`
- UI components + tick labels: `src/ui/ui-components.hpp`, `src/ui/ui-components.cpp`
- Replay buffer manager: `src/managers/replay-buffer-manager.hpp`, `src/managers/replay-buffer-manager.cpp`
- Settings manager: `src/managers/settings-manager.hpp`, `src/managers/settings-manager.cpp`
- Save button settings: `src/managers/save-button-settings.hpp`, `src/managers/save-button-settings.cpp`
- Hotkey manager: `src/managers/hotkey-manager.hpp`, `src/managers/hotkey-manager.cpp`
- Utilities: `src/utils/obs-utils.*`, `src/utils/logger.hpp`, `src/utils/video-trimmer.*`
- Config constants: `src/config/config.hpp`
- Localization: `data/locale/en-US.ini`
- Build system: `CMakeLists.txt`, `buildspec.json`, `CMakePresets.json`, `cmake/`
- CI/CD: `.github/workflows/`, `.github/actions/`, `.github/scripts/`

## Core runtime flows
### Buffer length update
1. Slider/spinbox/tick label changes value in the dock.
2. Debounce timer expires.
3. `SettingsManager::updateBufferLengthSettings(...)` writes `RecRBTime` into OBS profile config.
4. If a replay output exists, updates `max_time_sec` and calls `obs_output_update(...)`.

### Save segment
1. User clicks a duration button or hotkey.
2. `ReplayBufferManager::saveSegment(...)` validates buffer active and duration <= current length.
3. `obs_frontend_replay_buffer_save()` is called and `pendingSaveDuration` is set.
4. On `OBS_FRONTEND_EVENT_REPLAY_BUFFER_SAVED`, the saved path is trimmed in a background thread.

### Save full buffer
1. User clicks “Save Replay Buffer”.
2. `ReplayBufferManager::saveFullBuffer(...)` saves without setting `pendingSaveDuration`.
3. Saved event occurs, but no trimming is performed.

## Key components and ownership
- `ReplayBufferPro::Plugin` (dock) owns UI, managers, timers, and OBS event wiring.
- `UIComponents` builds the UI and manages enabled/disabled state.
- `ReplayBufferManager` handles save requests and trimming.
- `SettingsManager` reads/writes OBS profile config and updates output settings.
- `HotkeyManager` registers per-duration hotkeys and persists bindings.
- `VideoTrimmer` trims using libavformat stream copy.

## Configuration and persistence
- Buffer length config key: `RecRBTime`.
- Config section is `AdvOut` for Advanced mode, otherwise `SimpleOutput`.
- Hotkey bindings are stored in `hotkey_bindings.json` under the module config path.
- Custom save button durations are stored in `save_button_settings.json` under the module config path.

## Build and localization
- Build system follows the [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate) pattern.
- Plugin metadata (name, version, author) and dependency versions are in `buildspec.json`.
- `CMakePresets.json` defines `windows-x64`, `macos` (universal), and `ubuntu-x86_64` configure/build presets.
- On Windows and macOS: OBS source, prebuilt obs-deps (with FFmpeg), and Qt6 are auto-downloaded into `.deps/` at configure time.
- On Linux: OBS, Qt6, and FFmpeg are installed from the system via apt and the OBS PPA; no deps are downloaded by CMake.
- FFmpeg on Windows/macOS comes from the prebuilt obs-deps archive. On Linux, FFmpeg is found via `pkg-config` (system packages).
- CMake modules live in `cmake/common/` (cross-platform), `cmake/windows/` (MSVC-specific), `cmake/macos/` (Xcode/macOS-specific), and `cmake/linux/` (GCC/Clang Linux-specific).
- Windows DLL embeds VERSIONINFO via `cmake/windows/resources/resource.rc.in`.
- macOS builds produce a `.plugin` bundle; packaging uses `pkgbuild`/`productbuild` to produce a `.pkg` installer.
- Linux builds install to `/usr/lib/<arch>-linux-gnu/obs-plugins/` and `/usr/share/obs/obs-plugins/`; packaging produces a `.deb` via CPack.
- Post-build rundir at `build_x64/rundir/<config>/` (Windows), `build_macos/rundir/<config>/` (macOS), or `build_x86_64/rundir/<config>/` (Linux) for quick testing.
- `prepare_release` custom CMake target creates a Windows zip package (Windows only; macOS and Linux packaging are handled by CI scripts).
- GitHub Actions CI: builds on push/PR for Windows, macOS, and Ubuntu 24.04; creates draft releases on semver tag push.
- Locale strings in `data/locale/en-US.ini` accessed with `obs_module_text(...)`.
- C++ source code is fully cross-platform — no platform `#ifdef` guards required; all OS interactions go through OBS APIs and FFmpeg.

### Build commands (Windows)
```bash
cmake --preset windows-x64          # Configure (downloads deps on first run)
cmake --build --preset windows-x64  # Build
cmake --install build_x64 --config RelWithDebInfo  # Install
```

### Build commands (macOS)
```bash
cmake --preset macos                 # Configure (downloads deps on first run; requires Xcode 16+)
cmake --build --preset macos         # Build
cmake --install build_macos --config RelWithDebInfo  # Install to ~/Library/Application Support/obs-studio/plugins/
```

### Build commands (Linux / Ubuntu)
```bash
# Install system dependencies first (requires OBS PPA):
sudo add-apt-repository ppa:obsproject/obs-studio
sudo apt update
sudo apt install build-essential cmake ninja-build pkg-config ccache \
  libavformat-dev libavcodec-dev libavutil-dev \
  obs-studio qt6-base-dev qt6-base-private-dev libqt6svg6-dev \
  libgles2-mesa-dev libsimde-dev

cmake --preset ubuntu-x86_64         # Configure
cmake --build --preset ubuntu-x86_64 # Build
cmake --install build_x86_64 --prefix /usr  # Install
```

## Not present
- No custom OBS sources, filters, or outputs are registered. The plugin uses OBS frontend replay buffer APIs.

## Documentation upkeep
- More documentation is available in `reference/` and README.md.
- Project website source lives in `docs/` and should be updated when relevant.
- Any changes to the project should be reflected in `reference/` for developers, `docs/` for plugin users and distribution, README.md for project introduction and setup, and this `AGENTS.md` file for agentic coding assistance.

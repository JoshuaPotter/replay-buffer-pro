# AGENTS

This file is a concise handoff for agents working in the Replay Buffer Pro OBS plugin.

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
- `CMakePresets.json` defines `windows-x64` configure/build presets (Visual Studio 17 2022).
- Dependencies (OBS source, prebuilt obs-deps with FFmpeg, Qt6) are auto-downloaded into `.deps/` at configure time.
- OBS (libobs + obs-frontend-api) is built from source during configure.
- FFmpeg (avformat, avcodec, avutil) comes from the prebuilt obs-deps archive.
- CMake modules live in `cmake/common/` (cross-platform) and `cmake/windows/` (MSVC-specific).
- Windows DLL embeds VERSIONINFO via `cmake/windows/resources/resource.rc.in`.
- Post-build rundir at `build_x64/rundir/<config>/` for quick testing.
- `prepare_release` custom target creates a zip package.
- GitHub Actions CI: builds on push/PR, creates draft releases on semver tag push.
- Locale strings in `data/locale/en-US.ini` accessed with `obs_module_text(...)`.

### Build commands (Windows)
```bash
cmake --preset windows-x64          # Configure (downloads deps on first run)
cmake --build --preset windows-x64  # Build
cmake --install build_x64 --config RelWithDebInfo  # Install
```

## Not present
- No custom OBS sources, filters, or outputs are registered. The plugin uses OBS frontend replay buffer APIs.

## Documentation upkeep
- More documentation is available in `reference/` and README.md.
- Project website source lives in `docs/` and should be updated when relevant.
- Any changes to the project should be reflected in `reference/` for developers, `docs/` for plugin users and distribution, README.md for project introduction and setup, and this `AGENTS.md` file for agentic coding assistance.

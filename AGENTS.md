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
- Utilities: `src/utils/obs-utils.*`, `src/utils/logger.hpp`, `src/utils/video-trimmer.*`, `src/utils/status-reporter.*`
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
3. The request is pushed onto a pending-save FIFO and `obs_frontend_replay_buffer_save()` is called.
4. On `OBS_FRONTEND_EVENT_REPLAY_BUFFER_SAVED`, `handleSaveCompleted(...)` pops the matching request and queues a trim job.
5. The manager's worker thread trims to a `.rbp-partial.<ext>` file, verifies its duration, renames it to `_trimmed`, then deletes the original.

### Save full buffer
1. User clicks “Save Replay Buffer”.
2. `ReplayBufferManager::saveFullBuffer(...)` queues a `0`-duration do-not-trim marker, then saves.
3. The saved event consumes that marker, so no trimming is performed and no stale duration can be inherited.

### Correlating saves
- OBS cannot tie a save request to the file it produces, so requests are matched to saved events in FIFO order.
- Requests expire after `Config::TRIM_REQUEST_TIMEOUT_MS` (OBS silently drops saves when encoders are paused).
- Requests within `Config::TRIM_REQUEST_COALESCE_MS` collapse into one, because OBS produces a single file for presses that close together.
- A saved event with nothing pending came from outside the plugin (OBS's own hotkey, tray, obs-websocket) and is logged but not trimmed.

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
- `CMakePresets.json` defines `windows-x64` and `macos` (universal) configure/build presets.
- Dependencies (OBS source, prebuilt obs-deps with FFmpeg, Qt6) are auto-downloaded into `.deps/` at configure time.
- OBS (libobs + obs-frontend-api) is built from source during configure.
- FFmpeg (avformat, avcodec, avutil) comes from the prebuilt obs-deps archive.
- CMake modules live in `cmake/common/` (cross-platform), `cmake/windows/` (MSVC-specific), and `cmake/macos/` (Xcode/macOS-specific).
- Windows DLL embeds VERSIONINFO via `cmake/windows/resources/resource.rc.in`.
- macOS builds produce a `.plugin` bundle; packaging uses `pkgbuild`/`productbuild` to produce a `.pkg` installer.
- Post-build rundir at `build_x64/rundir/<config>/` (Windows) or `build_macos/rundir/<config>/` (macOS) for quick testing.
- `prepare_release` custom CMake target creates a Windows zip package (Windows only; macOS packaging is handled by the CI `package-macos` script).
- GitHub Actions CI: builds on push/PR for Windows and macOS, creates draft releases on semver tag push.
- macOS CI uses Xcode's built-in compilation cache (CAS), not ccache — a ccache compiler-wrapper triggers a "conflicting deployment targets" error under Xcode 26.
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
cmake --preset macos                 # Configure (downloads deps on first run; requires Xcode 26.5+)
cmake --build --preset macos         # Build
cmake --install build_macos --config RelWithDebInfo  # Install to ~/Library/Application Support/obs-studio/plugins/
```

## Not present
- No custom OBS sources, filters, or outputs are registered. The plugin uses OBS frontend replay buffer APIs.

## Documentation upkeep
- More documentation is available in `reference/` and README.md.
- Project website source lives in `docs/` and should be updated when relevant.
- See `.claude/rules/keep-docs-updated.md` for the rule on keeping `README.md`, `reference/`, `docs/`, and this file in sync with project changes.

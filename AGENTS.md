# AGENTS

This file is a concise handoff for agents working in the Replay Buffer Pro OBS plugin.

## Project summary
- Adds a dockable OBS UI for replay buffer controls.
- Lets users adjust replay buffer length and save clips of customizable durations.
- Trims saved replays to the last N seconds using FFmpeg libavformat (no re-encode).

## Architecture map (start here)
- Module entry + OBS integration: `src/main.cpp`
- Dock widget + UI orchestration: `src/plugin/plugin.hpp`, `src/plugin/plugin.cpp`
- UI components: `src/ui/ui-components.hpp`, `src/ui/ui-components.cpp`
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
1. User steps or types a new value into the buffer length spinbox in the dock.
2. Debounce timer expires.
3. `SettingsManager::updateBufferLengthSettings(...)` writes `RecRBTime` into OBS profile config.
4. If a replay output exists, updates `max_time_sec` and calls `obs_output_update(...)`.

### Save segment
1. User clicks a duration button or hotkey.
2. `ReplayBufferManager::saveSegment(...)` validates buffer active and duration <= current length.
3. `requestSave(...)` refuses the save if OBS would drop it (buffer inactive, recording paused), otherwise arms it with `obs_frontend_replay_buffer_save()`, folds it into a not-yet-started outstanding request, or defers it behind a file being written.
4. On the replay buffer output's own `saved` signal (mux thread), the manager reads the path and posts it to `handleSaveCompleted(...)` on the Qt main thread, which queues a trim job and issues any deferred request.
5. The manager's worker thread trims to a `.rbp-partial.<ext>` file, verifies its duration, renames it to `_trimmed`, then deletes the original.

### Save full buffer
1. User clicks “Save Replay Buffer”.
2. `ReplayBufferManager::saveFullBuffer(...)` requests a `0`-duration save, an explicit do-not-trim marker.
3. The completion carrying that marker is logged `save-full-buffer` and left untrimmed.

### Correlating saves
- OBS holds a single armed save timestamp (`save_ts`) that a later request overwrites, so it never queues requests; a FIFO of requests drifts out of step with files. See `reference/architecture/replay-buffer-flow.md` for the OBS source citations.
- The manager keeps at most one outstanding request plus one deferred request (last-write-wins). A press before OBS starts writing folds into the outstanding request; a press while a file is being written is deferred; a press while a foreign save is being written waits behind a placeholder.
- A pre-flight gate mirrors OBS's own drop conditions (output inactive, video encoder paused), so a save OBS would drop is refused before any state exists.
- Completions come from the output's `saved` signal, not `OBS_FRONTEND_EVENT_REPLAY_BUFFER_SAVED`, which OBS suppresses when the buffer stopped mid-write or during scene collection/profile switches. Do not handle both, or every file is trimmed twice.
- The subscription is level-triggered via `ensureSubscribed()`, called on `REPLAY_BUFFER_STARTED`, every save request and every watchdog tick. It only acts while the buffer is active: OBS has no output handler at module load and the frontend API dereferences it unchecked, and an active buffer guarantees it exists. It keeps listening to an old output that still owes the outstanding request a file.
- A held request (deferred or waiting-foreign) is issued late, so its file ends after the key press. When it is issued, the manager records `heldNs`: the recorded time since the press, i.e. the wall-clock wait minus time recording was paused (`obs_encoder_get_pause_offset`), because paused time is not in the file. The trim job's `endOffsetSeconds` makes `VideoTrimmer::trimToWindow` end the clip at the press. Requests issued at once have exactly 0. A press older than the whole buffer fails with `window-not-in-buffer`; Save Replay Buffer saves are never trimmed back.
- A liveness-checked watchdog only releases requests OBS never started a file for. It never bounds how long a write takes (issue #40).
- The `saved` callback runs on the mux thread and must never block; `ensureSubscribed()` must disconnect before replacing the held output.
- A saved signal with nothing outstanding came from outside the plugin (OBS's own hotkey, tray, obs-websocket) and is logged `no-pending-request` but not trimmed.

## Key components and ownership
- `ReplayBufferPro::Plugin` (dock) owns UI, managers, timers, and OBS event wiring.
- `UIComponents` builds the UI and manages enabled/disabled state.
- `ReplayBufferManager` handles save requests and trimming, and owns the replay buffer output reference and its `saved` signal subscription.
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
- No custom OBS sources, filters, or outputs are registered. The plugin uses OBS frontend replay buffer APIs to save, and connects directly to the replay buffer output's `saved` signal and `get_last_replay` proc to learn when and where each file was written.

## Documentation upkeep
- More documentation is available in `reference/` and README.md.
- Project website source lives in `docs/` and should be updated when relevant.
- See `.claude/rules/keep-docs-updated.md` for the rule on keeping `README.md`, `reference/`, `docs/`, and this file in sync with project changes.

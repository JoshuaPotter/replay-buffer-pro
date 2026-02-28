# Replay Buffer Pro Documentation

This folder documents the Replay Buffer Pro OBS Studio plugin. The plugin adds a dockable UI panel that controls the built-in OBS replay buffer, and trims saved files to a selected duration using FFmpeg libraries.

## Architecture map
If you are new to the plugin, read in order:
1. Module entry + OBS integration: `reference/architecture/module-and-obs-integration.md`
2. UI layer (dock + widgets): `reference/architecture/ui-layer.md`
3. Replay buffer flow (save + trim): `reference/architecture/replay-buffer-flow.md`
4. Settings + hotkeys: `reference/architecture/settings-and-hotkeys.md`
5. Utilities + shared helpers: `reference/architecture/utilities.md`
6. Build + localization: `reference/architecture/build-and-localization.md`

## Runtime overview
1. OBS loads the module and registers the dock widget.
2. The dock creates UI controls (slider, spinbox, save buttons, tick labels).
3. Buffer length changes update OBS profile config and replay output settings.
4. Save actions trigger OBS replay buffer save.
5. When OBS emits the saved event, the plugin trims the saved file in a background thread.

## Key components and state
- `ReplayBufferPro::Plugin` is the dock widget; it owns managers, UI, and timers.
- `ReplayBufferManager` stores a pending save duration to decide if trimming is needed.
- `SettingsManager` reads/writes OBS profile config (`RecRBTime`).
- `SaveButtonSettings` loads/saves custom save button durations globally.
- `HotkeyManager` registers per-button hotkeys and persists bindings to JSON.

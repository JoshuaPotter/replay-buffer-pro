# Settings and Hotkeys

This document describes how replay buffer length settings are stored, how save button durations are persisted, and how hotkeys are registered.

## Settings management
### Responsibilities
- Read the replay buffer length from the OBS profile config.
- Update the correct config section depending on output mode.
- Push the new value into the replay output settings.

### Config context selection
- `SettingsManager::getConfigContext()` reads `Output.Mode`.
- If the mode is `Advanced`, the section is `AdvOut`.
- Otherwise the section is `SimpleOutput`.

### Buffer length updates
1. `updateBufferLengthSettings(seconds)` reads current value and returns if unchanged.
2. Writes `RecRBTime` into the appropriate section.
3. Saves the config with `config_save(...)` and `obs_frontend_save()`.
4. If a replay output exists, updates `max_time_sec` via `obs_output_update(...)`.

### Reads
- `getCurrentBufferLength()` returns the current value of `RecRBTime` for the active output mode.

### Related constants
- `Config::MIN_BUFFER_LENGTH` and `Config::MAX_BUFFER_LENGTH` control UI range.
- `Config::DEFAULT_BUFFER_LENGTH` is used when OBS has no stored value.
- `Config::SETTINGS_MONITOR_INTERVAL` controls how often the dock polls settings.

## Plugin properties
### Responsibilities
- `PluginProperties` loads and saves everything the properties window edits, globally (module config path): the save buttons (1 to 12, each a duration and an optional label), the in-memory clips toggle, and the window's size.
- Normalize values to stay within valid bounds.

### Persistence
- Properties are stored in `properties.json` under the module config path.
- When `properties.json` doesn't exist, `load()` reads `save_button_settings.json`, the file's earlier name. Saves only write `properties.json` and leave the old file in place, so an older version still finds its settings.
- `properties_window_width` and `properties_window_height` hold the size the user last left the window at; they are absent while it is the default size.
- Schema version 2 stores a version, a `save_buttons` array of `{seconds, label}` (its length is the button count), and the `in_memory_clips` boolean (the properties window's "Write clips straight from memory" checkbox, off when missing). Version 1 files (six buttons, no labels) load unchanged.
- Invalid or missing data falls back to default durations.

## Hotkeys
### Responsibilities
- Register a hotkey per save button in use.
- Register or unregister button hotkeys when the button count changes (`setButtons(...)`).
- Save hotkey bindings to a module config JSON file.
- Load hotkey bindings after registration.
- Read and replace a button's bindings for the properties window (`getAllBindings()`, `setBinding(...)`).
- Read and replace OBS's own Save Replay hotkey for the properties window's Save Replay Buffer row (target `FULL_BUFFER`). See below.
- Unregister every hotkey in its destructor. OBS holds its hotkey lock while a callback runs, so none is running once that returns.

### Hotkey registration details
- Hotkey name format: `ReplayBufferPro.SaveButton{index}`.
- Description format: "Replay Buffer Pro: Save <duration>", or "Replay Buffer Pro: <label>" for a labelled button (`formatButtonHotkeyDescription(...)`).
- The OBS frontend reroutes hotkey callbacks to the Qt main thread (`OBSBasic::HotkeyTriggered` → `ProcessHotkey` → `obs_hotkey_trigger_routed_callback`), which runs them while holding libobs's hotkey lock. Each callback only posts the press back to the event loop (the dock is the context object), so the save, and any warning dialog it shows, runs after the lock is released instead of stalling libobs's hotkey thread. The posted press then maps the hotkey ID back to the current duration for that index; a button removed before it runs no longer matches.

### Persistence
- `saveHotkeySettings()` writes bindings to `hotkey_bindings.json` under the module config path, on exit and whenever the properties window applies a hotkey change.
- Uses OBS data APIs: `obs_hotkey_save(...)`, `obs_data_set_array(...)`, and `obs_data_save_json_safe(...)`.
- Keys are `hotkey_{index}` per button.
- Saving starts from the existing file, so a button removed by lowering the count keeps its `hotkey_{index}` entry; registering it again loads it back.
- `loadHotkeySettings(...)` loads the JSON and calls `obs_hotkey_load(...)` for the given hotkeys.

### OBS's Save Replay hotkey
- The properties window's Save Replay Buffer row edits OBS's own Save Replay hotkey, so it and OBS **Settings → Hotkeys → Save Replay** always show the same key. The plugin registers no full-buffer hotkey of its own, so one key never saves twice.
- OBS registers it on the replay buffer output (`ReplayBuffer.Save`, `obs-ffmpeg-mux.c`), which exists only while the replay buffer is on in Output settings, and keeps its bindings in the profile config under `[Hotkeys] ReplayBuffer`, read when OBS creates the output.
- `getAllBindings()` reads the output's bindings (`obs_hotkeys_save_output(...)`), or the profile config while there is no output.
- `setBinding(FULL_BUFFER, ...)` finds the hotkey's ID with `obs_enum_hotkeys(...)` (registered by that output, named `ReplayBuffer.Save`), replaces its bindings, and writes the output's bindings to the profile config the way `OBSBasicSettings::SaveHotkeySettings` does, because OBS writes them only when its own Settings are applied. Without an output it edits the profile config alone.
- Both need the frontend's output handler, so they run only from the properties window, after OBS has loaded.
- A press of it is OBS's own save: the plugin logs it `no-pending-request` and leaves it untrimmed, the same as the dock's Save Replay Buffer button's result.

### Properties window access
- Targets are a button index or `HotkeyManager::FULL_BUFFER` (the same index as `PropertiesDialog::FULL_BUFFER_HOTKEY`).
- `getAllBindings()` returns one entry per button slot, then `FULL_BUFFER`. It enumerates `obs_enum_hotkey_bindings(...)` for each registered hotkey ID and, for a button that isn't registered, decodes its saved `hotkey_{index}` entry from `hotkey_bindings.json`, read once per call.
- `setBinding(index, combination)` calls `obs_hotkey_load_bindings(...)` with one combination, or none to clear. It replaces every binding the hotkey had, so the properties window only calls it for rows the user changed. A changed row hidden by lowering the button count is not applied and stays pending until it is shown and applied.
- Bindings set in OBS Settings → Hotkeys and in the properties window are the same libobs bindings.

## Key classes and functions
- `SettingsManager::getConfigContext()`
- `SettingsManager::updateBufferLengthSettings(...)`
- `SettingsManager::getCurrentBufferLength()`
- `PluginProperties::load()`
- `PluginProperties::save()`
- `HotkeyManager::registerHotkeys()`
- `HotkeyManager::saveHotkeySettings()`
- `HotkeyManager::loadHotkeySettings()`

## Related code
- `src/managers/settings-manager.hpp`
- `src/managers/settings-manager.cpp`
- `src/managers/plugin-properties.hpp`
- `src/managers/plugin-properties.cpp`
- `src/managers/hotkey-manager.hpp`
- `src/managers/hotkey-manager.cpp`
- `src/config/config.hpp`

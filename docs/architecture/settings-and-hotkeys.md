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

## Save buttons
### Responsibilities
- Load and save the save buttons (1 to 12, each a duration and an optional label) globally (module config path).
- Normalize values to stay within valid bounds.

### Persistence
- Settings are stored in `save_button_settings.json` under the module config path.
- Schema version 2 stores a version, a `save_buttons` array of `{seconds, label}` (its length is the button count), and the `in_memory_clips` boolean (the properties window's "Write clips straight from memory" checkbox, off when missing). Version 1 files (six buttons, no labels) load unchanged.
- Invalid or missing data falls back to default durations.

## Hotkeys
### Responsibilities
- Register a hotkey per save button in use, and one for Save Replay Buffer (`ReplayBufferPro.SaveFullBuffer`).
- Register or unregister button hotkeys when the button count changes (`setButtons(...)`).
- Save hotkey bindings to a module config JSON file.
- Load hotkey bindings after registration.
- Read and replace a button's bindings for the properties window (`getBindings(...)`, `setBinding(...)`).

### Hotkey registration details
- Hotkey name format: `ReplayBufferPro.SaveButton{index}`.
- Description format: "Replay Buffer Pro: Save <duration>", or "Replay Buffer Pro: <label>" for a labelled button (`formatButtonHotkeyDescription(...)`).
- Callback maps the pressed hotkey ID back to the current duration for that index.

### Persistence
- `saveHotkeySettings()` writes bindings to `hotkey_bindings.json` under the module config path, on exit and whenever the properties window applies a hotkey change.
- Uses OBS data APIs: `obs_hotkey_save(...)`, `obs_data_set_array(...)`, and `obs_data_save_json_safe(...)`.
- Keys are `hotkey_{index}` per button and `hotkey_full` for Save Replay Buffer.
- Saving starts from the existing file, so a button removed by lowering the count keeps its `hotkey_{index}` entry; registering it again loads it back.
- `loadHotkeySettings(...)` loads the JSON and calls `obs_hotkey_load(...)` for the given hotkeys.

### Properties window access
- Targets are a button index or `HotkeyManager::FULL_BUFFER` (the same index as `PropertiesDialog::FULL_BUFFER_HOTKEY`).
- `getBindings(target)` enumerates `obs_enum_hotkey_bindings(...)` for that hotkey ID, or, for a button that isn't registered, decodes its saved `hotkey_{index}` entry.
- `setBinding(index, combination)` calls `obs_hotkey_load_bindings(...)` with one combination, or none to clear. It replaces every binding the hotkey had, so the properties window only calls it for rows the user changed.
- Bindings set in OBS Settings → Hotkeys and in the properties window are the same libobs bindings.

## Key classes and functions
- `SettingsManager::getConfigContext()`
- `SettingsManager::updateBufferLengthSettings(...)`
- `SettingsManager::getCurrentBufferLength()`
- `SaveButtonSettings::load()`
- `SaveButtonSettings::save()`
- `HotkeyManager::registerHotkeys()`
- `HotkeyManager::saveHotkeySettings()`
- `HotkeyManager::loadHotkeySettings()`

## Related code
- `src/managers/settings-manager.hpp`
- `src/managers/settings-manager.cpp`
- `src/managers/save-button-settings.hpp`
- `src/managers/save-button-settings.cpp`
- `src/managers/hotkey-manager.hpp`
- `src/managers/hotkey-manager.cpp`
- `src/config/config.hpp`

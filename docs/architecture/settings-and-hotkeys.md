# Settings and Hotkeys

This document describes how replay buffer length settings are stored and how hotkeys are registered and persisted.

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

## Hotkeys
### Responsibilities
- Register a hotkey per save button duration.
- Save hotkey bindings to a module config JSON file.
- Load hotkey bindings after registration.

### Hotkey registration details
- Hotkey name format: `ReplayBufferPro.Save{duration}Sec`.
- Description format: `Save {label}` where `{label}` comes from localization.
- Callback maps the pressed hotkey ID back to the configured duration.

### Persistence
- `saveHotkeySettings()` writes bindings to `hotkey_bindings.json` under the module config path.
- Uses OBS data APIs: `obs_hotkey_save(...)`, `obs_data_set_array(...)`, and `obs_data_save_json_safe(...)`.
- `loadHotkeySettings()` loads the JSON and calls `obs_hotkey_load(...)` per hotkey.

## Key classes and functions
- `SettingsManager::getConfigContext()`
- `SettingsManager::updateBufferLengthSettings(...)`
- `SettingsManager::getCurrentBufferLength()`
- `HotkeyManager::registerHotkeys()`
- `HotkeyManager::saveHotkeySettings()`
- `HotkeyManager::loadHotkeySettings()`

## Related code
- `src/managers/settings-manager.hpp`
- `src/managers/settings-manager.cpp`
- `src/managers/hotkey-manager.hpp`
- `src/managers/hotkey-manager.cpp`
- `src/config/config.hpp`

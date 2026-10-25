# UI Layer (Dock + Widgets)

This document describes the dockable UI panel and the widget components that control replay buffer behavior.

## Dock panel responsibilities
- Host the replay buffer controls inside an OBS dock (`ReplayBufferPro::Plugin`).
- Wire UI events to settings and replay save actions.
- Observe OBS frontend events and update UI state.
- Periodically reload buffer length from OBS settings.

## UI composition
The dock content is wrapped in a padded `QFrame` (`replayBufferProFrame`, `NoFrame` shape), mirroring the `controlsFrame`/`scenesFrame` pattern native OBS docks use to get a consistent gutter between the dock's border and its content. Inside it, the dock assembles a vertical layout that includes:
- Subtitle label (`WidgetTitle`).
- Buffer length header with a label and a seconds spinbox (`QSpinBox`, with its up/down step buttons enabled).
- Save clip section title and a grid of save buttons.
- A footer `QToolBar` (`UIComponents::createFooter(...)`) spanning the frame edge to edge, with a properties (gear) button on the right. Built like OBS's Audio Mixer footer: inside a dock the theme's `OBSDock QToolBar` rules give it the top border and button styling, and the `icon-gear` class on the action's widget gives the theme's gear icon, with OBS's `:/settings/images/settings/general.svg` as the fallback.

## UI controls and behavior
### Buffer length controls
- A single `QSpinBox` is the buffer length control (no separate slider or tick-label row).
- A debounce timer (`Config::BUFFER_LENGTH_DEBOUNCE_INTERVAL`) prevents frequent OBS config updates while the value is being stepped/typed.
- When the replay buffer is active, the spinbox is disabled.
- Clicking or typing into the disabled spinbox triggers a warning dialog via an event filter.

### Save buttons
- Save buttons are generated from the current customizable duration settings.
- Buttons are arranged in a grid, 3 per row.
- 1 to 12 buttons (`SaveButtonSettings`); `UIComponents::setSaveButtons(...)` deletes and rebuilds them in the grid when the count, durations or labels change.
- A button shows its custom label, or "Last <duration>" when it has none (`formatButtonLabel(...)` in `src/utils/duration-format.*`); a labelled button gets the duration as its tooltip.
- A full buffer save button spans the final row.
- Buttons are enabled only when the current buffer length is at least the duration they save.

## Properties window
`PropertiesDialog` (`src/ui/properties-dialog.*`) holds every plugin option, one page per area, with a page list on the left and OK / Cancel / Apply, styled after OBS Settings. New options get a page or a row here, not a dialog of their own. OBS has no API for adding a page to its own Settings window.
- Opened by the dock's footer properties button (Save Buttons page) and by **Tools → Replay Buffer Pro Properties** (`obs_frontend_add_tools_menu_qaction(...)`), modal over the OBS main window, through `Plugin::openProperties(...)`.
- **Save Buttons:** a Number of buttons spin box (1–12), then one row per slot (all 12 are built and the count shows or hides them) with duration, label (placeholder shows the default text), a hotkey field (`HotkeyEdit`) and a Clear button, and a Save Replay Buffer row with its own hotkey. Hidden rows show the bindings saved for those buttons, so raising the count shows what they will get back.
- **About:** name, version (`PLUGIN_VERSION`, a compile definition from `buildspec.json`), links to the usage guide, changelog, website and issues, and a tip on attaching the OBS log.
- **Experimental:** the "Write clips straight from memory" checkbox.
- OK and Apply emit `applied()`; `Plugin::applyProperties(...)` stores the values and refreshes the dock buttons, hotkey names and bindings. Cancel discards edits made since the last Apply.
- While the window is open, OBS's background hotkeys are paused with `obs_hotkey_enable_background_press(false)`, as OBS does for its own Settings window, so pressing a bound key to rebind it does not save a clip. Closing restores OBS's rule from the "Hotkey focus behavior" setting (`HotkeyFocusType`). If the user switches away from OBS and back while the window is open, OBS reapplies that rule itself.

### Hotkey field
`HotkeyEdit` (`src/ui/hotkey-edit.*`) is modelled on OBS's `OBSHotkeyEdit`, which the frontend API does not export. It records one key combination with `obs_key_from_virtual_key(...)` and the same modifier mapping as OBS, plus the middle and extra mouse buttons. Each button shows its first binding; editing a row replaces all of that hotkey's bindings with one, and untouched rows keep any extra bindings set in OBS Settings → Hotkeys.

## Event and state flow
1. User steps or types a new value into the buffer length spinbox.
2. `UIComponents::updateBufferLengthValue(...)` updates the spinbox and save button enabled states.
3. Dock restarts the debounce timer and waits for input to settle.
4. On debounce timeout, `SettingsManager::updateBufferLengthSettings(...)` persists the value.
5. OBS frontend events update UI enabled/disabled state and reload buffer length when needed.

## Configuration inputs
- Spinbox range uses `Config::MIN_BUFFER_LENGTH` and `Config::MAX_BUFFER_LENGTH`.
- Debounce interval uses `Config::BUFFER_LENGTH_DEBOUNCE_INTERVAL`.
- Save buttons are generated from `SaveButtonSettings`; `Config::MAX_SAVE_BUTTON_COUNT`, `Config::DEFAULT_SAVE_BUTTON_COUNT` and `Config::SAVE_BUTTON_DEFAULTS` (starting durations per slot) bound them.

## Key classes and functions
- `ReplayBufferPro::Plugin` (dock widget)
- `ReplayBufferPro::UIComponents::createUI()`
- `ReplayBufferPro::UIComponents::updateBufferLengthValue(...)`
- `ReplayBufferPro::UIComponents::updateBufferLengthState(...)`

## Related code
- `src/plugin/plugin.hpp`
- `src/plugin/plugin.cpp`
- `src/ui/ui-components.hpp`
- `src/ui/ui-components.cpp`
- `src/ui/properties-dialog.hpp`
- `src/ui/properties-dialog.cpp`
- `src/ui/hotkey-edit.hpp`
- `src/ui/hotkey-edit.cpp`

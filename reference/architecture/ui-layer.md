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
- Save clip section title, customize button, and a grid of save buttons.

## UI controls and behavior
### Buffer length controls
- A single `QSpinBox` is the buffer length control (no separate slider or tick-label row).
- A debounce timer (`Config::BUFFER_LENGTH_DEBOUNCE_INTERVAL`) prevents frequent OBS config updates while the value is being stepped/typed.
- When the replay buffer is active, the spinbox is disabled.
- Clicking or typing into the disabled spinbox triggers a warning dialog via an event filter.

### Save buttons
- Save buttons are generated from the current customizable duration settings.
- Buttons are arranged in a grid, 3 per row.
- A full buffer save button spans the final row.
- Buttons are enabled only when the current buffer length is at least the duration they save.
- A customize button opens a dialog to edit per-button durations.

## Event and state flow
1. User steps or types a new value into the buffer length spinbox.
2. `UIComponents::updateBufferLengthValue(...)` updates the spinbox and save button enabled states.
3. Dock restarts the debounce timer and waits for input to settle.
4. On debounce timeout, `SettingsManager::updateBufferLengthSettings(...)` persists the value.
5. OBS frontend events update UI enabled/disabled state and reload buffer length when needed.

## Configuration inputs
- Spinbox range uses `Config::MIN_BUFFER_LENGTH` and `Config::MAX_BUFFER_LENGTH`.
- Debounce interval uses `Config::BUFFER_LENGTH_DEBOUNCE_INTERVAL`.
- Save buttons are generated from `SaveButtonSettings` and `Config::SAVE_BUTTON_COUNT`.

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

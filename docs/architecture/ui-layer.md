# UI Layer (Dock + Widgets)

This document describes the dockable UI panel and the widget components that control replay buffer behavior.

## Dock panel responsibilities
- Host the replay buffer controls inside an OBS dock (`ReplayBufferPro::Plugin`).
- Wire UI events to settings and replay save actions.
- Observe OBS frontend events and update UI state.
- Periodically reload buffer length from OBS settings.

## UI composition
The dock assembles a vertical layout that includes:
- Subtitle label (`WidgetTitle`).
- Buffer length header with a label and seconds input (`QSpinBox`).
- Buffer length slider (`QSlider`).
- Tick label widget for quick duration selection.
- Divider line.
- Save clip section title and a grid of save buttons.

## UI controls and behavior
### Buffer length controls
- `QSlider` and `QSpinBox` are bound to the same value.
- A debounce timer (`Config::SLIDER_DEBOUNCE_INTERVAL`) prevents frequent OBS config updates.
- When the replay buffer is active, the slider and spinbox are disabled.
- Clicking disabled controls triggers a warning dialog via an event filter.

### Tick label widget
- `TickLabelWidget` renders time labels such as `5m`, `1h`, `6h` on the slider axis.
- It dynamically shows or hides labels based on available width.
- Labels are clickable and update the buffer length value.
- When the replay buffer is active, clicking labels shows a warning and does not update settings.

### Save buttons
- Save buttons are generated from `Config::SAVE_BUTTONS`.
- Buttons are arranged in a grid, 3 per row.
- A full buffer save button spans the final row.
- Buttons are enabled only when the current buffer length is at least the duration they save.

## Event and state flow
1. User adjusts slider/spinbox or clicks a tick label.
2. `UIComponents::updateBufferLengthValue(...)` syncs slider and spinbox.
3. Dock restarts the debounce timer and waits for input to settle.
4. On debounce timeout, `SettingsManager::updateBufferLengthSettings(...)` persists the value.
5. OBS frontend events update UI enabled/disabled state and reload buffer length when needed.

## Configuration inputs
- Slider range uses `Config::MIN_BUFFER_LENGTH` and `Config::MAX_BUFFER_LENGTH`.
- Debounce interval uses `Config::SLIDER_DEBOUNCE_INTERVAL`.
- Save buttons are generated from `Config::SAVE_BUTTONS` and `Config::SAVE_BUTTON_COUNT`.

## Key classes and functions
- `ReplayBufferPro::Plugin` (dock widget)
- `ReplayBufferPro::UIComponents::createUI()`
- `ReplayBufferPro::UIComponents::updateBufferLengthValue(...)`
- `ReplayBufferPro::UIComponents::updateBufferLengthState(...)`
- `ReplayBufferPro::TickLabelWidget` (event filter, resize and show events)

## Related code
- `src/plugin/plugin.hpp`
- `src/plugin/plugin.cpp`
- `src/ui/ui-components.hpp`
- `src/ui/ui-components.cpp`

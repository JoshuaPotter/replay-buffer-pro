# Module Entry and OBS Integration

This document covers how the plugin integrates with OBS, how the module lifecycle works, and where the dock is registered.

## Responsibilities
- Declare the OBS module and set the default locale.
- Create the dock widget and register it with OBS.
- Log module lifecycle events.
- Provide the bridge between OBS module hooks and the Qt widget.

## Entry points and hooks
- `OBS_DECLARE_MODULE()` registers the plugin as an OBS module.
- `OBS_MODULE_USE_DEFAULT_LOCALE("replay-buffer-pro", "en-US")` wires localization.
- `obs_module_load()` logs that the plugin loaded.
- `obs_module_post_load()` instantiates the dock widget and registers it with OBS.
- `obs_module_unload()` logs unload and clears the global pointer (OBS deletes the dock).

## Dock registration
- `obs_module_post_load()` creates a `ReplayBufferPro::Plugin` widget.
- The dock is registered with `obs_frontend_add_dock_by_id("replay-buffer-pro", "Replay Buffer Pro", pluginInstance)`.
- OBS owns the widget lifetime once the dock is added.

## Dock initialization sequence
When the dock widget is constructed:
1. Managers are created (`ReplayBufferManager`, `SettingsManager`).
2. UI components are created and mounted in a `QVBoxLayout`.
3. Slider and spinbox signals are connected with a shared debounce timer.
4. Current buffer length is loaded from OBS settings.
5. OBS frontend event callback is registered.
6. Hotkeys are registered.
7. A timer starts polling OBS settings to keep UI state in sync.

## OBS frontend events handled
- `OBS_FRONTEND_EVENT_EXIT`: stop settings timer and save hotkeys.
- `OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTING`: stop settings timer and disable buffer length controls.
- `OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED`: re-enable controls and reload settings.
- `OBS_FRONTEND_EVENT_REPLAY_BUFFER_SAVED`: initiate trimming for segment saves.

## OBS frontend integration
The module itself does not register OBS sources or outputs. Instead it relies on frontend APIs:
- `obs_frontend_add_dock_by_id(...)` for the dock.
- `obs_frontend_add_event_callback(...)` inside the dock to react to replay buffer events.
- `obs_frontend_replay_buffer_*` APIs for replay buffer operations (see replay flow doc).

## Notes
- The module stores a global `ReplayBufferPro::Plugin*` pointer only for module scope; OBS manages widget destruction.
- No custom OBS sources, filters, or outputs are registered in this plugin.

## Related code
- `src/main.cpp`
- `src/plugin/plugin.hpp`
- `src/plugin/plugin.cpp`

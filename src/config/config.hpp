#pragma once

/**
 * @brief Configuration constants for the Replay Buffer Pro plugin
 */
namespace ReplayBufferPro
{
  namespace Config
  {
    /**
     * @brief Defines a save button configuration
     */
    struct SaveButton
    {
      int duration;     ///< Duration in seconds to save
      const char *text; ///< Translation key for button text
    };

    /**
     * @brief Predefined save durations and their labels
     */
    constexpr SaveButton SAVE_BUTTONS[] = {
        {15, "Save15Sec"},  // 15 seconds
        {30, "Save30Sec"},  // 30 seconds
        {60, "Save1Min"},   // 1 minute
        {300, "Save5Min"},  // 5 minutes
        {600, "Save10Min"}, // 10 minutes
        {900, "Save15Min"}, // 15 minutes
        {1800, "Save30Min"} // 30 minutes
    };

    /**
     * @brief Number of predefined save buttons
     */
    constexpr size_t SAVE_BUTTON_COUNT = sizeof(SAVE_BUTTONS) / sizeof(SAVE_BUTTONS[0]);

    // Buffer length configuration
    constexpr int MIN_BUFFER_LENGTH = 10;      // 10 seconds minimum
    constexpr int MAX_BUFFER_LENGTH = 21600;   // 6 hours maximum
    constexpr int DEFAULT_BUFFER_LENGTH = 300; // 5 minutes default

    // Configuration keys
    constexpr const char *REPLAY_BUFFER_LENGTH_KEY = "RecRBTime";
    constexpr const char *DOCK_AREA_KEY = "DockArea";
    constexpr const char *DOCK_GEOMETRY_KEY = "DockGeometry";
    constexpr const char *HOTKEY_BINDINGS_KEY = "HotkeyBindings";

    // Timer intervals
    constexpr int SETTINGS_MONITOR_INTERVAL = 1000; // 1 second
    constexpr int SLIDER_DEBOUNCE_INTERVAL = 800;   // 800 milliseconds

    // File paths
    constexpr const char *DOCK_STATE_FILENAME = "dock_state.json";
    constexpr const char *TEMP_FILE_SUFFIX = "tmp";
    constexpr const char *BACKUP_FILE_SUFFIX = "bak";
  } // namespace Config
} // namespace ReplayBufferPro
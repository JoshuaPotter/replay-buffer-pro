#pragma once

/**
 * @brief Configuration constants for the Replay Buffer Pro plugin
 */
namespace ReplayBufferPro
{
  namespace Config
  {
    /**
     * @brief Default save button durations in seconds
     */
    constexpr int SAVE_BUTTONS[] = {15, 30, 60, 300, 900, 1800};

    /**
     * @brief Number of save buttons
     */
    constexpr size_t SAVE_BUTTON_COUNT = sizeof(SAVE_BUTTONS) / sizeof(int);

    // Buffer length configuration
    constexpr int MIN_BUFFER_LENGTH = 1;      // 1 seconds minimum
    constexpr int MAX_BUFFER_LENGTH = 21600;   // 6 hours maximum (OBS built-in limit)
    constexpr int DEFAULT_BUFFER_LENGTH = 300; // 5 minutes default

    // Configuration keys
    constexpr const char *REPLAY_BUFFER_LENGTH_KEY = "RecRBTime";
    constexpr const char *HOTKEY_BINDINGS_KEY = "HotkeyBindings";

    // Timer intervals
    constexpr int SETTINGS_MONITOR_INTERVAL = 1000; // 1 second
    constexpr int SLIDER_DEBOUNCE_INTERVAL = 800;   // 800 milliseconds

    // File paths
    constexpr const char *TEMP_FILE_SUFFIX = "tmp";
    constexpr const char *BACKUP_FILE_SUFFIX = "bak";
  } // namespace Config
} // namespace ReplayBufferPro

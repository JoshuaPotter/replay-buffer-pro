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
    constexpr int SETTINGS_MONITOR_INTERVAL = 1000;       // 1 second
    constexpr int BUFFER_LENGTH_DEBOUNCE_INTERVAL = 800;   // 800 milliseconds

    // Trim request correlation
    constexpr int TRIM_REQUEST_TIMEOUT_MS = 30000;  // Drop a request OBS never honored
    constexpr int TRIM_REQUEST_COALESCE_MS = 250;   // Presses this close yield one OBS file

    // Trim retry behavior. OBS starts AutoRemux on the same file the moment it fires
    // the saved event, so the first open or unlink can lose a race with it.
    constexpr int TRIM_OPEN_RETRY_COUNT = 5;
    constexpr int TRIM_OPEN_RETRY_DELAY_MS = 250; // Backs off from here
    constexpr int TRIM_UNLINK_RETRY_COUNT = 5;
    constexpr int TRIM_UNLINK_RETRY_DELAY_MS = 200;

    // Trim output validation
    constexpr double TRIM_KEYFRAME_TOLERANCE_SECONDS = 10.0; // Warn past this cut-point drift
    constexpr double TRIM_MIN_DURATION_RATIO = 0.5;          // Below this the output is truncated
    constexpr double TRIM_MAX_DURATION_RATIO = 2.0;          // Above this AND the slack, it collapsed
    constexpr double TRIM_MAX_DURATION_SLACK_SECONDS = 15.0;

    // File paths
    constexpr const char *TEMP_FILE_SUFFIX = "tmp";
    constexpr const char *BACKUP_FILE_SUFFIX = "bak";
    constexpr const char *TRIM_PARTIAL_SUFFIX = ".rbp-partial";
    constexpr const char *TRIM_OUTPUT_SUFFIX = "_trimmed";
  } // namespace Config
} // namespace ReplayBufferPro

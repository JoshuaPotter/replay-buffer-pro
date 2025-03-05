#pragma once

/**
 * @brief Configuration constants for the Replay Buffer Pro plugin
 */
namespace ReplayBufferPro
{
  namespace Config
  {
    // Buffer length configuration
    constexpr int MIN_BUFFER_LENGTH = 10;      // 10 seconds minimum
    constexpr int MAX_BUFFER_LENGTH = 21600;   // 6 hours maximum
    constexpr int DEFAULT_BUFFER_LENGTH = 300; // 5 minutes default

    // Configuration keys
    constexpr const char *REPLAY_BUFFER_LENGTH_KEY = "RecRBTime";
    constexpr const char *DOCK_AREA_KEY = "DockArea";
    constexpr const char *DOCK_GEOMETRY_KEY = "DockGeometry";

    // Timer intervals
    constexpr int SETTINGS_MONITOR_INTERVAL = 1000; // 1 second
    constexpr int SLIDER_DEBOUNCE_INTERVAL = 800;   // 800 milliseconds

    // File paths
    constexpr const char *DOCK_STATE_FILENAME = "dock_state.json";
    constexpr const char *TEMP_FILE_SUFFIX = "tmp";
    constexpr const char *BACKUP_FILE_SUFFIX = "bak";
  } // namespace Config
} // namespace ReplayBufferPro
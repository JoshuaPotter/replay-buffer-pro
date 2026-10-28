#pragma once

/**
 * @brief Configuration constants for the Replay Buffer Pro plugin
 */
namespace ReplayBufferPro
{
  namespace Config
  {
    /**
     * @brief Starting duration in seconds for each save button slot
     *
     * The first DEFAULT_SAVE_BUTTON_COUNT are the default buttons; the rest
     * are what a button starts at when the user raises the button count.
     */
    constexpr int SAVE_BUTTON_DEFAULTS[] = {15, 30, 60, 300, 900, 1800, 180, 600, 3600, 7200, 10800, 21600};

    /**
     * @brief Most save buttons the dock can show
     */
    constexpr size_t MAX_SAVE_BUTTON_COUNT = sizeof(SAVE_BUTTON_DEFAULTS) / sizeof(int);

    /**
     * @brief Number of save buttons a new install shows
     */
    constexpr size_t DEFAULT_SAVE_BUTTON_COUNT = 6;

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

    // Save-completion watchdog.
    //
    // Correlation is event-driven: a request is resolved by the replay buffer
    // output's own "saved" signal, cleared when the buffer stops, and refused up
    // front when OBS would drop it. Nothing here bounds how long OBS may take to
    // write a file; that can legitimately run for minutes (issue #40). The
    // watchdog only catches saves that will never produce a "saved" signal at
    // all (a mux pipe failure, a stalled encoder), and it cannot fire while OBS
    // is still writing, because it checks for an in-flight mux first.
    constexpr int SAVE_WATCHDOG_GRACE_MS = 15000;         // Bounds save -> mux start, NOT mux duration
    constexpr int SAVE_WATCHDOG_PROBE_INTERVAL_MS = 5000; // Re-check spacing while OBS is writing
    constexpr int SAVE_MUX_STALL_WARN_MS = 60000;         // Log-only; never clears state

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

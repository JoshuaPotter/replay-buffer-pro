/**
 * @file status-reporter.hpp
 * @brief Posts transient messages to the OBS main window status bar
 * @author Joshua Potter
 * @copyright GPL v2 or later
 *
 * Trimming runs on a worker thread, so messages are marshalled onto the Qt
 * main thread before touching the status bar.
 */

#pragma once

// Qt includes
#include <QString>

namespace ReplayBufferPro
{

  /**
   * @brief Shows short-lived messages in the OBS status bar
   */
  class StatusReporter
  {
  public:
    /**
     * @brief Default display time for a success message
     */
    static constexpr int DEFAULT_TIMEOUT_MS = 5000;

    /**
     * @brief Display time for a failure message, which is worth reading
     */
    static constexpr int FAILURE_TIMEOUT_MS = 10000;

    /**
     * @brief Shows a message in the OBS status bar
     * @param message Text to display
     * @param timeoutMs How long the message stays visible
     *
     * Safe to call from any thread. Does nothing if the OBS main window is
     * unavailable, which is the case during shutdown.
     */
    static void showMessage(const QString &message, int timeoutMs = DEFAULT_TIMEOUT_MS);
  };

} // namespace ReplayBufferPro

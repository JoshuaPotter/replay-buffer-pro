/**
 * @file replay-buffer-manager.hpp
 * @brief Manages replay buffer operations for the plugin
 * @author Joshua Potter
 * @copyright GPL v2 or later
 */

#pragma once

// OBS includes
#include <obs-module.h>
#include <obs-frontend-api.h>

// STL includes
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>

// Qt includes
#include <QObject>
#include <QMessageBox>

// Local includes
#include "utils/video-trimmer.hpp"

namespace ReplayBufferPro
{
  /**
   * @brief Manages replay buffer operations including saving and trimming
   *
   * OBS gives no way to correlate a save request with the file it eventually
   * produces, so requests are tracked in a FIFO and matched to saved events in
   * order. Requests expire, which keeps one that OBS silently dropped from
   * being applied to somebody else's clip later on.
   */
  class ReplayBufferManager : public QObject
  {
    Q_OBJECT

  public:
    //=========================================================================
    // CONSTRUCTORS & DESTRUCTOR
    //=========================================================================
    /**
     * @brief Constructor
     * @param parent Parent QObject
     */
    explicit ReplayBufferManager(QObject *parent = nullptr);

    /**
     * @brief Destructor, drains and joins the trim worker
     */
    ~ReplayBufferManager();

    //=========================================================================
    // REPLAY BUFFER OPERATIONS
    //=========================================================================
    /**
     * @brief Saves the replay buffer and queues a trim for the resulting file
     * @param duration Seconds to save
     * @param parent Parent widget for error messages
     * @return Success status
     */
    bool saveSegment(int duration, QWidget *parent = nullptr);

    /**
     * @brief Saves the entire replay buffer without trimming
     * @param parent Parent widget for error messages
     * @return Success status
     */
    bool saveFullBuffer(QWidget *parent = nullptr);

    /**
     * @brief Matches a completed save to its request and schedules the trim
     *
     * Call from the Qt main thread when OBS reports a replay buffer save. Emits
     * exactly one verdict line to the log for every save, whatever the outcome.
     *
     * @param savedPath Path OBS reported for the saved replay, may be empty
     */
    void handleSaveCompleted(const std::string &savedPath);

  private:
    //=========================================================================
    // TYPES
    //=========================================================================
    /**
     * @brief A save this plugin asked for, awaiting its saved event
     */
    struct PendingSave
    {
      int duration;           ///< Seconds to keep, or 0 for an untrimmed full save
      uint64_t requestedAtNs; ///< When it was requested, for expiry and coalescing
    };

    /**
     * @brief A file waiting to be trimmed on the worker thread
     */
    struct TrimJob
    {
      std::string sourcePath;
      int duration;
    };

    //=========================================================================
    // REQUEST TRACKING
    //=========================================================================
    /**
     * @brief Records a save request, expiring stale ones and coalescing bursts
     * @param duration Seconds to keep, or 0 for an untrimmed full save
     */
    void enqueueRequest(int duration);

    /**
     * @brief Removes and returns the oldest live request, if any
     * @return The request, or nullopt when the save did not come from this plugin
     */
    std::optional<PendingSave> takeNextPendingSave();

    /**
     * @brief Pops requests OBS never honored off the front of pendingSaves
     * @param now Current timestamp, as returned by os_gettime_ns()
     * @pre pendingMutex is held by the caller
     */
    void expireStaleRequestsLocked(uint64_t now);

    //=========================================================================
    // TRIMMING
    //=========================================================================
    /**
     * @brief Worker thread body, trims queued files one at a time
     */
    void workerLoop();

    /**
     * @brief Trims one file, verifies the result, then replaces the original
     * @param job File and duration to process
     */
    void processTrimJob(const TrimJob &job);

    /**
     * @brief Checks a freshly written trim is plausibly the requested length
     * @param outputPath File to probe
     * @param duration Requested duration in seconds
     * @param sourceDuration Duration of the input, which caps what is achievable
     * @param actualDuration Receives the measured duration
     * @param reason Receives a short failure token when the check fails
     * @return true if the output should be kept
     */
    static bool verifyTrimmedOutput(const std::string &outputPath, int duration,
                                    double sourceDuration, double *actualDuration,
                                    std::string *reason);

    //=========================================================================
    // HELPER METHODS
    //=========================================================================
    /**
     * @brief Gets the final path for a trimmed file
     * @param sourcePath Original file path
     * @return Trimmed file path
     */
    static std::string getTrimmedOutputPath(const std::string &sourcePath);

    /**
     * @brief Gets the scratch path a trim is written to before it is verified
     * @param sourcePath Original file path
     * @return Partial file path
     */
    static std::string getPartialOutputPath(const std::string &sourcePath);

    /**
     * @brief Deletes a file, retrying while another process still holds it
     * @param path File to delete
     * @return true if the file is gone
     */
    static bool removeFileWithRetry(const std::string &path);

    /**
     * @brief Logs the single verdict line for a save and shows a status message
     * @param outcome One of "ok", "skipped" or "failed"
     * @param detail Trailing key=value diagnostics for the log line
     * @param statusMessage Status bar text, empty to show nothing
     * @param isFailure Whether to use the longer failure status timeout
     */
    static void reportVerdict(const char *outcome, const std::string &detail,
                              const QString &statusMessage, bool isFailure);

    //=========================================================================
    // MEMBER VARIABLES
    //=========================================================================
    std::mutex pendingMutex;              ///< Guards pendingSaves
    std::deque<PendingSave> pendingSaves; ///< Requests awaiting a saved event

    std::mutex jobMutex;             ///< Guards jobQueue and stopping
    std::condition_variable jobCv;   ///< Signals the worker
    std::deque<TrimJob> jobQueue;    ///< Files awaiting trimming
    std::thread worker;              ///< Owned so trims cannot outlive the manager
    bool stopping = false;           ///< Tells the worker to drain and exit
  };

} // namespace ReplayBufferPro

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
#include <obs.hpp>

// STL includes
#include <atomic>
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
#include <QTimer>

// Local includes
#include "utils/video-trimmer.hpp"

namespace ReplayBufferPro
{
  /**
   * @brief Manages replay buffer operations including saving and trimming
   *
   * OBS gives no way to tie a save request to the file it produces, and it does
   * not queue requests either: the replay buffer holds a single armed save
   * timestamp (save_ts in obs-ffmpeg-mux.c) that a later request overwrites, so
   * any number of requests made before OBS starts writing yield one file and one
   * saved signal. A FIFO of requests therefore drifts out of step with the files.
   *
   * This manager mirrors that model instead. It keeps at most one outstanding
   * request (issued to OBS, awaiting its file) and one deferred request (made
   * while a file was already being written, issued once that write completes).
   * A press before OBS has started writing folds into the outstanding request,
   * exactly as OBS folds it into save_ts. Requests OBS would silently drop are
   * refused before any state is recorded, and completions come from the replay
   * buffer output's own "saved" signal, which fires once per written file.
   *
   * All correlation state lives on the Qt main thread.
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
     * Runs on the Qt main thread, posted from the replay buffer output's
     * "saved" signal. Emits exactly one verdict line to the log for every
     * saved file, whatever the outcome.
     *
     * @param savedPath Path of the file OBS just finished, may be empty
     */
    void handleSaveCompleted(const std::string &savedPath);

    /**
     * @brief Reacts to replay buffer lifecycle events
     *
     * Call from the Qt main thread for REPLAY_BUFFER_STARTED (subscribe, so
     * saves from outside the plugin are logged) and REPLAY_BUFFER_STOPPED
     * (clear requests OBS can no longer honor). Other events are ignored.
     *
     * @param event The frontend event that occurred
     */
    void handleFrontendEvent(obs_frontend_event event);

    /**
     * @brief Disconnects from OBS and abandons any live request
     *
     * Idempotent. Call on OBS_FRONTEND_EVENT_EXIT, while the frontend API is
     * still usable; the destructor calls it again as a backstop.
     */
    void shutdown();

  private:
    //=========================================================================
    // TYPES
    //=========================================================================
    /**
     * @brief A save awaiting its file
     *
     * A foreign placeholder stands for a save something else (OBS's own
     * hotkey, a Stream Deck OBS action, obs-websocket) was already writing
     * when this plugin was asked to save. It holds our request back until that
     * file has been accounted for, so the foreign file is not trimmed as ours.
     */
    struct SaveRequest
    {
      int duration = 0;           ///< Seconds to keep, or 0 for an untrimmed full save
      uint64_t requestedAtNs = 0; ///< When the key was pressed; the clip should end here
      uint64_t pauseAtRequestNs = 0; ///< Encoder pause offset at the press, to discount pauses
      uint64_t heldNs = 0;        ///< Recorded time between press and issue; 0 if issued at once
      uint64_t armedAtNs = 0;     ///< When save was issued to OBS (or the placeholder made)
      uint64_t graceStartNs = 0;  ///< Watchdog: start of the grace period, restarted by pauses
      uint64_t generation = 0;    ///< Monotonic id, only for log correlation
      bool foreign = false;       ///< Placeholder for a save this plugin did not issue
      bool stallWarned = false;   ///< Watchdog: long-write warning already logged
    };

    /**
     * @brief A file waiting to be trimmed on the worker thread
     */
    struct TrimJob
    {
      std::string sourcePath;
      int duration;
      double endOffsetSeconds; ///< How far before the file's end the clip should end
    };

    //=========================================================================
    // REQUEST TRACKING (Qt main thread)
    //=========================================================================
    /**
     * @brief Records a validated save request and issues, folds or defers it
     * @param duration Seconds to keep, or 0 for an untrimmed full save
     * @return false if OBS would have dropped the save, so nothing was recorded
     */
    bool requestSave(int duration);

    /**
     * @brief Creates a request stamped with the current time as its press time
     */
    static SaveRequest newRequest(int duration, uint64_t generation = 0);

    /**
     * @brief Total time the replay buffer's video encoder has spent paused
     *
     * Paused time produces no frames, so it is absent from a saved file's
     * timeline and must not count towards how far back a held press lies.
     */
    static uint64_t encoderPauseOffsetNs();

    /**
     * @brief Issues a save to OBS and makes it the outstanding request
     */
    void arm(const SaveRequest &request);

    /**
     * @brief Makes a request outstanding and starts its watchdog grace period
     */
    void setOutstanding(SaveRequest request);

    /**
     * @brief Holds a request back behind a file this plugin did not ask for
     * @param next The request to issue once that file has been accounted for
     */
    void waitBehindForeign(const SaveRequest &next);

    /**
     * @brief Issues the deferred request, if any, once nothing is outstanding
     */
    void promoteDeferred();

    /**
     * @brief Emits the verdict for a finished file and queues its trim
     * @param request The request the file belongs to
     * @param savedPath Path of the finished file
     */
    void dispatchCompletion(const SaveRequest &request, const std::string &savedPath);

    /**
     * @brief Mirrors OBS clearing its armed save when the buffer stops
     *
     * A request whose file OBS has already started is kept, because that file
     * still completes and signals after the buffer stops.
     */
    void handleBufferStopped();

    /**
     * @brief Drops every live request, logging a verdict for each
     * @param reason Reason token for the verdict lines
     */
    void abandonAll(const char *reason);

    /**
     * @brief Checks the conditions under which OBS silently drops a save
     *
     * Mirrors replay_buffer_hotkey() in obs-ffmpeg-mux.c against the current
     * replay buffer output. Has no side effects.
     *
     * @return nullptr if OBS would honor a save now, else the reason token
     */
    static const char *armRefusalReason();

    /**
     * @brief Logs the verdict for a request that will not get a file
     * @param reason Reason token
     * @param request The request being dropped
     * @param isDeferred Whether it was the deferred request
     * @param extra Trailing key=value diagnostics, starting with a space
     */
    static void reportSkipped(const char *reason, const SaveRequest &request, bool isDeferred,
                              const std::string &extra = std::string());

    /**
     * @brief Whether OBS has already started producing a file
     *
     * True while a mux is in flight, or when a file has finished but its
     * completion is still queued for the main thread. A request made while
     * this is false folds into the pending save_ts; one made while it is true
     * gets a file of its own.
     */
    bool isFileStarted();

    /**
     * @brief Asks the subscribed output for the path of the file it last wrote
     *
     * get_last_replay only reports a path while the output is not muxing.
     *
     * @return nullopt while a mux is in flight, otherwise the path (empty if
     *         OBS has none, there is no output, or it lacks the proc)
     */
    std::optional<std::string> queryLastReplay();

    //=========================================================================
    // OBS SIGNAL SUBSCRIPTION
    //=========================================================================
    /**
     * @brief Connects the saved signal to the current replay buffer output
     *
     * Level-triggered: compares against the output held last time, so it
     * recovers from any lifecycle event OBS suppressed (frontend events are
     * dropped wholesale during scene collection and profile switches). Only
     * acts while the buffer is active, which is also what guarantees OBS's
     * output handler exists. Keeps listening to the old output while it still
     * owes the outstanding request a file. Qt main thread only.
     */
    void ensureSubscribed();

    /**
     * @brief Replay buffer output "saved" signal handler
     *
     * Runs on OBS's mux thread. Must never block or wait on the main thread:
     * signal_handler_disconnect() holds the same mutex while it waits for this
     * callback, so blocking here would deadlock teardown.
     */
    static void onSavedSignal(void *data, calldata_t *params);

    /**
     * @brief Liveness check for the outstanding request
     *
     * Single-shot: runs once the grace period is up, then reschedules itself
     * for as long as the request is waiting on OBS.
     */
    void onWatchdogTick();

    /**
     * @brief Logs a correlation state transition
     */
    static void logState(const char *transition, const SaveRequest &request,
                         const std::string &extra = std::string());

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
     * @param availableSeconds Source time up to the clip's end, which caps what is achievable
     * @param actualDuration Receives the measured duration
     * @param reason Receives a short failure token when the check fails
     * @return true if the output should be kept
     */
    static bool verifyTrimmedOutput(const std::string &outputPath, int duration,
                                    double availableSeconds, double *actualDuration,
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
    // Correlation state, Qt main thread only
    std::optional<SaveRequest> outstanding; ///< Issued to OBS, awaiting its file
    std::optional<SaveRequest> deferred;    ///< Waiting for outstanding; never set without it
    uint64_t nextGeneration = 1;            ///< Source of SaveRequest::generation
    bool shutDown = false;                  ///< Set once shutdown() has run

    // Saved-signal subscription. subscribedOutput is also read from the mux
    // thread inside onSavedSignal; that is safe only because it is never
    // reassigned while savedSignal is connected (see ensureSubscribed).
    OBSOutputAutoRelease subscribedOutput;  ///< Strong ref to the output we listen to
    OBSSignal savedSignal;                  ///< Connection to its "saved" signal

    // Completions counted on the mux thread vs. handled on the main thread.
    // seen > handled means a file is finished but not yet processed.
    std::atomic<uint64_t> savedEdgesSeen{0};
    uint64_t savedEdgesHandled = 0;

    QTimer *watchdog = nullptr;             ///< Single-shot; armed only while a request is outstanding

    std::mutex jobMutex;             ///< Guards jobQueue and stopping
    std::condition_variable jobCv;   ///< Signals the worker
    std::deque<TrimJob> jobQueue;    ///< Files awaiting trimming
    std::thread worker;              ///< Owned so trims cannot outlive the manager
    bool stopping = false;           ///< Tells the worker to drain and exit
  };

} // namespace ReplayBufferPro

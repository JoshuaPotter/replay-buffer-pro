/**
 * @file replay-buffer-manager.cpp
 * @brief Implementation of replay buffer management for the Replay Buffer Pro plugin
 */

#include "managers/replay-buffer-manager.hpp"
#include "managers/settings-manager.hpp"
#include "config/config.hpp"
#include "utils/duration-format.hpp"
#include "utils/logger.hpp"
#include "utils/status-reporter.hpp"
#include "utils/video-trimmer.hpp"

// OBS includes
#include <util/config-file.h>
#include <util/platform.h>

// Qt includes
#include <QMessageBox>
#include <QString>

// STL includes
#include <algorithm>
#include <chrono>

namespace ReplayBufferPro
{
  namespace
  {
    /**
     * @brief Splits a path into everything before the extension and the extension
     *
     * Only considers a dot that falls inside the file name, so a directory such
     * as "C:/clips.old/replay" is not mistaken for an extension.
     */
    void splitExtension(const std::string &path, std::string *stem, std::string *extension)
    {
      const size_t separator = path.find_last_of("/\\");
      const size_t nameStart = (separator == std::string::npos) ? 0 : separator + 1;
      const size_t dot = path.find_last_of('.');

      if (dot != std::string::npos && dot > nameStart)
      {
        *stem = path.substr(0, dot);
        *extension = path.substr(dot);
      }
      else
      {
        *stem = path;
        extension->clear();
      }
    }

    /**
     * @brief Quotes a path for a log line
     */
    std::string quoted(const std::string &value)
    {
      return "'" + value + "'";
    }
  } // namespace

  //=============================================================================
  // CONSTRUCTORS & DESTRUCTOR
  //=============================================================================

  ReplayBufferManager::ReplayBufferManager(QObject *parent)
      : QObject(parent)
  {
    // Not subscribed here: the manager is built in obs_module_post_load, before
    // OBS creates its output handler, and asking for the replay buffer output
    // that early dereferences a null handler. FINISHED_LOADING and every save
    // request subscribe instead.
    watchdog = new QTimer(this);
    watchdog->setInterval(Config::SAVE_WATCHDOG_INTERVAL_MS);
    connect(watchdog, &QTimer::timeout, this, [this]() { onWatchdogTick(); });

    worker = std::thread([this]() { workerLoop(); });
  }

  ReplayBufferManager::~ReplayBufferManager()
  {
    shutdown();

    {
      std::lock_guard<std::mutex> lock(jobMutex);
      stopping = true;
    }
    jobCv.notify_all();

    if (worker.joinable())
    {
      worker.join();
    }
  }

  //=============================================================================
  // REPLAY BUFFER OPERATIONS
  //=============================================================================

  bool ReplayBufferManager::saveSegment(int duration, QWidget *parent)
  {
    if (!obs_frontend_replay_buffer_active())
    {
      if (parent)
      {
        QMessageBox::warning(parent, obs_module_text("Warning"),
                             obs_module_text("ReplayBufferNotActive"));
      }
      return false;
    }

    SettingsManager settingsManager;
    int currentBufferLength = settingsManager.getCurrentBufferLength();

    if (duration > currentBufferLength)
    {
      if (parent)
      {
        QMessageBox::warning(parent, obs_module_text("Warning"),
                             QString(obs_module_text("CannotSaveSegment"))
                                 .arg(duration)
                                 .arg(currentBufferLength));
      }
      return false;
    }

    return requestSave(duration);
  }

  bool ReplayBufferManager::saveFullBuffer(QWidget *parent)
  {
    if (obs_frontend_replay_buffer_active())
    {
      // A 0 duration is an explicit do-not-trim marker. It occupies the
      // request slot like any other save, so the resulting file is never
      // attributed a trim duration.
      return requestSave(0);
    }
    else if (parent)
    {
      QMessageBox::warning(parent, obs_module_text("Error"),
                           obs_module_text("ReplayBufferNotActive"));
    }
    return false;
  }

  void ReplayBufferManager::handleFrontendEvent(obs_frontend_event event)
  {
    switch (event)
    {
    case OBS_FRONTEND_EVENT_FINISHED_LOADING:
      frontendReady = true;
      ensureSubscribed();
      break;
    case OBS_FRONTEND_EVENT_PROFILE_CHANGED:
    case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTING:
    case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED:
    case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPING:
      ensureSubscribed();
      break;
    case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED:
      // Only STOPPED clears state: while the buffer is merely stopping, packets
      // still flow and an armed save can still fire.
      ensureSubscribed();
      handleBufferStopped();
      break;
    default:
      break;
    }
  }

  void ReplayBufferManager::shutdown()
  {
    if (shutDown)
    {
      return;
    }
    shutDown = true;

    if (watchdog)
    {
      watchdog->stop();
    }

    // Waits for a saved callback already running on the mux thread, so none
    // is running or can start once this returns.
    savedSignal.Disconnect();
    subscribedOutput = static_cast<obs_output_t *>(nullptr);

    abandonAll("shutdown");
  }

  //=============================================================================
  // REQUEST TRACKING
  //=============================================================================

  bool ReplayBufferManager::requestSave(int duration)
  {
    if (shutDown)
    {
      return false;
    }

    // Refusing here, before anything is recorded, is what keeps a save OBS
    // would silently drop from lingering and claiming somebody else's file.
    const ArmVerdict verdict = checkArmable();
    if (verdict != ArmVerdict::Ok)
    {
      reportVerdict("skipped",
                    std::string("reason=") + armVerdictReason(verdict) +
                        " requested=" + std::to_string(duration) + "s",
                    QString(), false);
      return false;
    }

    const uint64_t generation = nextGeneration++;

    if (!outstanding)
    {
      if (isFileStarted())
      {
        // Something else is writing (or has just written) a file this plugin
        // did not ask for. Its saved signal will arrive first, so hold ours
        // back until that file has been accounted for.
        SaveRequest placeholder;
        placeholder.foreign = true;
        placeholder.armedAtNs = os_gettime_ns();
        outstanding = placeholder;
        lastProbeNs = 0;
        muxStallWarned = false;

        deferred = SaveRequest{duration, 0, generation, false};
        logState("waiting-foreign", *deferred);
        syncWatchdog();
        return true;
      }

      arm(duration, generation);
      return true;
    }

    if (!outstanding->foreign && !isFileStarted())
    {
      // OBS has not started our file yet, so another save() would only
      // overwrite the same save_ts. Fold the press into the outstanding
      // request instead; the newest press decides the duration.
      const int previous = outstanding->duration;
      outstanding->duration = duration;
      logState("folded", *outstanding, " previous=" + std::to_string(previous) + "s");
      return true;
    }

    // A file is being written. OBS would hold this press until that write
    // finishes anyway; holding it here keeps each file paired with its request.
    const std::string replaced =
        deferred ? " replaced=" + std::to_string(deferred->generation) : std::string(" replaced=none");
    deferred = SaveRequest{duration, 0, generation, false};
    logState("deferred", *deferred, replaced);
    return true;
  }

  void ReplayBufferManager::arm(int duration, uint64_t generation)
  {
    // State first: save() is synchronous from the main thread, so the request
    // must already be outstanding if anything reacts to it.
    outstanding = SaveRequest{duration, os_gettime_ns(), generation, false};
    lastProbeNs = 0;
    muxStallWarned = false;

    logState("armed", *outstanding);
    obs_frontend_replay_buffer_save();
    syncWatchdog();
  }

  void ReplayBufferManager::promoteDeferred()
  {
    if (outstanding || !deferred)
    {
      syncWatchdog();
      return;
    }

    SaveRequest next = *deferred;
    deferred.reset();

    const ArmVerdict verdict = checkArmable();
    if (verdict != ArmVerdict::Ok)
    {
      reportVerdict("skipped",
                    std::string("reason=") + armVerdictReason(verdict) +
                        " requested=" + std::to_string(next.duration) + "s deferred",
                    QString(), false);
      syncWatchdog();
      return;
    }

    if (isFileStarted())
    {
      // A foreign save got in between our completion and now
      SaveRequest placeholder;
      placeholder.foreign = true;
      placeholder.armedAtNs = os_gettime_ns();
      outstanding = placeholder;
      lastProbeNs = 0;
      muxStallWarned = false;

      deferred = next;
      logState("waiting-foreign", next);
      syncWatchdog();
      return;
    }

    logState("promoted", next);
    arm(next.duration, next.generation);
  }

  void ReplayBufferManager::abandonAll(const char *reason)
  {
    if (outstanding && !outstanding->foreign)
    {
      reportVerdict("skipped",
                    std::string("reason=") + reason +
                        " requested=" + std::to_string(outstanding->duration) + "s",
                    QString(), false);
    }

    if (deferred)
    {
      reportVerdict("skipped",
                    std::string("reason=") + reason +
                        " requested=" + std::to_string(deferred->duration) + "s deferred",
                    QString(), false);
    }

    outstanding.reset();
    deferred.reset();
    syncWatchdog();
  }

  void ReplayBufferManager::handleBufferStopped()
  {
    // Nothing can arm once the buffer is gone
    if (deferred)
    {
      reportVerdict("skipped",
                    "reason=replay-buffer-stopped requested=" +
                        std::to_string(deferred->duration) + "s deferred",
                    QString(), false);
      deferred.reset();
    }

    // OBS zeroes an armed save_ts on stop, but a file it has already started
    // still completes and signals, so only an unstarted request is dropped.
    if (outstanding && !isFileStarted())
    {
      if (!outstanding->foreign)
      {
        reportVerdict("skipped",
                      "reason=replay-buffer-stopped requested=" +
                          std::to_string(outstanding->duration) + "s",
                      QString(), false);
      }
      else
      {
        logState("cleared", *outstanding, " reason=replay-buffer-stopped");
      }
      outstanding.reset();
    }

    syncWatchdog();
  }

  ReplayBufferManager::ArmVerdict ReplayBufferManager::checkArmable()
  {
    // Also guarantees we listen to the exact output about to be armed, even if
    // OBS suppressed the lifecycle events that would normally tell us.
    ensureSubscribed();

    obs_output_t *rb = subscribedOutput.Get();
    if (!rb || !obs_output_active(rb))
    {
      return ArmVerdict::BufferInactive;
    }

    // The video encoder is borrowed from the output, not referenced
    if (obs_encoder_paused(obs_output_get_video_encoder(rb)))
    {
      return ArmVerdict::EncoderPaused;
    }

    return ArmVerdict::Ok;
  }

  const char *ReplayBufferManager::armVerdictReason(ArmVerdict verdict)
  {
    switch (verdict)
    {
    case ArmVerdict::EncoderPaused:
      return "encoder-paused";
    case ArmVerdict::BufferInactive:
    default:
      return "buffer-inactive";
    }
  }

  bool ReplayBufferManager::isFileStarted()
  {
    return isMuxInFlight() || savedEdgesSeen.load() > savedEdgesHandled;
  }

  bool ReplayBufferManager::isMuxInFlight()
  {
    obs_output_t *rb = subscribedOutput.Get();
    if (!rb)
    {
      return false;
    }

    calldata_t cd = {0};
    const bool called = proc_handler_call(obs_output_get_proc_handler(rb), "get_last_replay", &cd);

    // get_last_replay always sets "path" (possibly to nothing) unless a mux is
    // in flight, so its absence is the signal
    const char *path = nullptr;
    const bool reported = calldata_get_string(&cd, "path", &path);
    calldata_free(&cd);

    // An output without the proc cannot tell us anything; treat it as idle so
    // requests are never held back forever
    return called && !reported;
  }

  //=============================================================================
  // OBS SIGNAL SUBSCRIPTION
  //=============================================================================

  void ReplayBufferManager::ensureSubscribed()
  {
    if (shutDown)
    {
      return;
    }

    // Before OBS finishes loading its output handler may not exist, and the
    // frontend API dereferences it unchecked. An active buffer implies it does.
    if (!frontendReady && !obs_frontend_replay_buffer_active())
    {
      return;
    }

    OBSOutputAutoRelease current(obs_frontend_get_replay_buffer_output());
    if (current.Get() == subscribedOutput.Get())
    {
      return;
    }

    const bool hadOutput = subscribedOutput.Get() != nullptr;

    // Disconnect BEFORE replacing the held output. Disconnect() waits for a
    // saved callback in progress, and that callback reads subscribedOutput, so
    // it must never observe the reassignment.
    savedSignal.Disconnect();
    subscribedOutput = std::move(current);

    if (subscribedOutput)
    {
      savedSignal.Connect(obs_output_get_signal_handler(subscribedOutput), "saved",
                          &ReplayBufferManager::onSavedSignal, this);
    }

    if (hadOutput && (outstanding || deferred))
    {
      abandonAll("output-replaced");
    }
  }

  void ReplayBufferManager::onSavedSignal(void *data, calldata_t *)
  {
    auto *self = static_cast<ReplayBufferManager *>(data);

    // Read the path here, on the mux thread, the instant the file is closed.
    // OBS's own copy (obs_frontend_get_last_replay) is only updated while the
    // buffer is still active, so after a stop it would name the previous clip.
    std::string path = self->readLastReplayPath();

    // Counted before posting, so the main thread can tell a finished file
    // apart from one that has not started yet
    self->savedEdgesSeen.fetch_add(1);

    // Never BlockingQueuedConnection: see onSavedSignal's declaration
    QMetaObject::invokeMethod(
        self, [self, path = std::move(path)]() { self->handleSaveCompleted(path); },
        Qt::QueuedConnection);
  }

  std::string ReplayBufferManager::readLastReplayPath()
  {
    obs_output_t *rb = subscribedOutput.Get();
    if (!rb)
    {
      return std::string();
    }

    calldata_t cd = {0};
    proc_handler_call(obs_output_get_proc_handler(rb), "get_last_replay", &cd);

    // calldata_string() returns NULL for a missing or empty key, so check both
    const char *path = nullptr;
    std::string result;
    if (calldata_get_string(&cd, "path", &path) && path)
    {
      result = path;
    }
    calldata_free(&cd);

    return result;
  }

  void ReplayBufferManager::onWatchdogTick()
  {
    if (!outstanding || shutDown)
    {
      syncWatchdog();
      return;
    }

    ensureSubscribed();
    if (!outstanding)
    {
      return;
    }

    // A stop whose frontend event OBS suppressed
    obs_output_t *rb = subscribedOutput.Get();
    if (!rb || !obs_output_active(rb))
    {
      handleBufferStopped();
      return;
    }

    const uint64_t now = os_gettime_ns();

    // Pausing after a save was armed does not retract it: OBS keeps save_ts and
    // fires it once packets flow again. Restart the grace period meanwhile, so
    // the save is not declared lost the moment recording resumes.
    if (obs_encoder_paused(obs_output_get_video_encoder(rb)))
    {
      outstanding->armedAtNs = now;
      lastProbeNs = 0;
      return;
    }

    const uint64_t elapsedNs = now - outstanding->armedAtNs;
    const uint64_t graceNs = static_cast<uint64_t>(Config::SAVE_WATCHDOG_GRACE_MS) * 1000000ULL;
    const uint64_t probeNs = static_cast<uint64_t>(Config::SAVE_WATCHDOG_PROBE_INTERVAL_MS) * 1000000ULL;

    // OBS starts writing within one encoded packet of a save; until the grace
    // period is up there is nothing to check.
    if (elapsedNs < graceNs)
    {
      return;
    }

    if (lastProbeNs != 0 && now - lastProbeNs < probeNs)
    {
      return;
    }
    lastProbeNs = now;

    const double elapsedSeconds = static_cast<double>(elapsedNs) / 1e9;

    if (isFileStarted())
    {
      // OBS is still writing the file. However long that takes, the request
      // stays; timing out here is exactly how issue #40 lost its trims.
      const uint64_t warnNs = static_cast<uint64_t>(Config::SAVE_MUX_STALL_WARN_MS) * 1000000ULL;
      if (!muxStallWarned && elapsedNs > warnNs)
      {
        Logger::warning("Replay buffer save still being written after %.0fs; waiting for OBS to finish",
                        elapsedSeconds);
        muxStallWarned = true;
      }
      return;
    }

    // OBS never started a file and is not writing one: the save was lost
    // (mux pipe failure, stalled encoder). Release the slot so the next
    // request is not stuck behind it.
    const SaveRequest dead = *outstanding;
    outstanding.reset();

    const std::string armed = QString::number(elapsedSeconds, 'f', 1).toStdString() + "s";
    if (!dead.foreign)
    {
      reportVerdict("skipped",
                    "reason=no-mux-started requested=" + std::to_string(dead.duration) +
                        "s armed=" + armed,
                    QString(), false);
    }
    else
    {
      logState("cleared", dead, " reason=foreign-save-never-completed armed=" + armed);
    }

    promoteDeferred();
  }

  void ReplayBufferManager::syncWatchdog()
  {
    if (!watchdog)
    {
      return;
    }

    if (outstanding && !shutDown)
    {
      if (!watchdog->isActive())
      {
        watchdog->start();
      }
    }
    else
    {
      watchdog->stop();
    }
  }

  void ReplayBufferManager::logState(const char *transition, const SaveRequest &request,
                                     const std::string &extra)
  {
    if (request.foreign)
    {
      Logger::info("SAVE STATE: %s gen=foreign%s", transition, extra.c_str());
      return;
    }

    Logger::info("SAVE STATE: %s gen=%llu duration=%ds%s", transition,
                 static_cast<unsigned long long>(request.generation), request.duration,
                 extra.c_str());
  }

  //=============================================================================
  // SAVE COMPLETION
  //=============================================================================

  void ReplayBufferManager::handleSaveCompleted(const std::string &savedPath)
  {
    savedEdgesHandled++;

    if (shutDown)
    {
      reportVerdict("skipped", "reason=shutdown file=" + quoted(savedPath), QString(), false);
      return;
    }

    if (!outstanding)
    {
      // OBS's own Save Replay hotkey, the tray item and obs-websocket all reach
      // here. Those saves are not ours to trim, but saying so explicitly is what
      // turns "my clip wasn't trimmed" into an answerable question.
      reportVerdict("skipped",
                    "reason=no-pending-request file=" + quoted(savedPath),
                    QString(), false);
      return;
    }

    const SaveRequest completed = *outstanding;
    outstanding.reset();
    lastProbeNs = 0;
    muxStallWarned = false;

    dispatchCompletion(completed, savedPath);

    // Only once this file is fully accounted for
    promoteDeferred();
  }

  void ReplayBufferManager::dispatchCompletion(const SaveRequest &request, const std::string &savedPath)
  {
    if (request.foreign)
    {
      // The save we were waiting out; ours is issued next
      reportVerdict("skipped",
                    "reason=no-pending-request file=" + quoted(savedPath),
                    QString(), false);
      return;
    }

    if (request.duration <= 0)
    {
      reportVerdict("skipped",
                    "reason=save-full-buffer file=" + quoted(savedPath),
                    QString(), false);
      return;
    }

    if (savedPath.empty())
    {
      reportVerdict("failed",
                    "reason=no-saved-path requested=" + std::to_string(request.duration) + "s",
                    QString(obs_module_text("StatusTrimFailed")).arg("no saved path"),
                    true);
      return;
    }

    // AutoRemux runs against this same file as soon as OBS handles the saved
    // signal, so note it up front when a trim later loses a race for the file.
    if (config_t *profile = obs_frontend_get_profile_config())
    {
      if (config_get_bool(profile, "Video", "AutoRemux"))
      {
        Logger::warning("OBS automatic remuxing is enabled; it competes with trimming "
                        "for the same file and can delay or block it");
      }
    }

    {
      std::lock_guard<std::mutex> lock(jobMutex);
      if (stopping)
      {
        reportVerdict("skipped",
                      "reason=shutdown file=" + quoted(savedPath) +
                          " requested=" + std::to_string(request.duration) + "s",
                      QString(), false);
        return;
      }
      jobQueue.push_back(TrimJob{savedPath, request.duration});
    }
    jobCv.notify_one();
  }

  //=============================================================================
  // TRIMMING
  //=============================================================================

  void ReplayBufferManager::workerLoop()
  {
    for (;;)
    {
      TrimJob job;

      {
        std::unique_lock<std::mutex> lock(jobMutex);
        jobCv.wait(lock, [this]() { return stopping || !jobQueue.empty(); });

        if (stopping)
        {
          if (!jobQueue.empty())
          {
            Logger::warning("Abandoning %zu queued trim(s) during shutdown; "
                            "those clips keep their full length",
                            jobQueue.size());
            jobQueue.clear();
          }
          return;
        }

        job = std::move(jobQueue.front());
        jobQueue.pop_front();
      }

      processTrimJob(job);
    }
  }

  void ReplayBufferManager::processTrimJob(const TrimJob &job)
  {
    const auto startedAt = std::chrono::steady_clock::now();

    const std::string partialPath = getPartialOutputPath(job.sourcePath);
    const std::string finalPath = getTrimmedOutputPath(job.sourcePath);
    const std::string requested = std::to_string(job.duration) + "s";

    auto elapsedSeconds = [&startedAt]() {
      return std::chrono::duration<double>(std::chrono::steady_clock::now() - startedAt).count();
    };

    auto fail = [&](const std::string &reason, const std::string &detail) {
      removeFileWithRetry(partialPath);

      std::string line = "reason=" + reason;
      if (!detail.empty())
      {
        line += " detail=" + quoted(detail);
      }
      line += " file=" + quoted(job.sourcePath) +
              " requested=" + requested +
              " elapsed=" + QString::number(elapsedSeconds(), 'f', 1).toStdString() + "s" +
              " original-kept";

      reportVerdict("failed", line,
                    QString(obs_module_text("StatusTrimFailed"))
                        .arg(QString::fromStdString(reason)),
                    true);
    };

    Logger::info("Trimming replay buffer save to %d seconds", job.duration);

    // A partial left by an earlier crash would otherwise fail the rename
    if (os_file_exists(partialPath.c_str()))
    {
      Logger::warning("Removing leftover partial file: %s", partialPath.c_str());
      removeFileWithRetry(partialPath);
    }

    const TrimResult trim = VideoTrimmer::trimToLastSeconds(job.sourcePath, partialPath, job.duration);
    if (!trim.success)
    {
      fail(trim.reason, trim.detail);
      return;
    }

    // Confirm the file on disk is the length that was asked for. Without this a
    // cut point that collapsed toward the start of the buffer would be reported
    // as a success and the full-length original deleted in its favour.
    double actualDuration = 0.0;
    std::string verifyReason;
    if (!verifyTrimmedOutput(partialPath, job.duration, trim.sourceDuration,
                             &actualDuration, &verifyReason))
    {
      Logger::error("Trimmed output failed verification (%s): %.2fs from a %.2fs source, "
                    "cut at %.2fs",
                    verifyReason.c_str(), actualDuration, trim.sourceDuration, trim.cutAt);
      fail(verifyReason, "measured " + QString::number(actualDuration, 'f', 1).toStdString() + "s");
      return;
    }

    // Only now is the trim allowed to take the final name, so nothing watching
    // the recordings folder ever sees a half-written clip.
    if (os_file_exists(finalPath.c_str()))
    {
      Logger::warning("Replacing existing file: %s", finalPath.c_str());
      removeFileWithRetry(finalPath);
    }

    if (os_rename(partialPath.c_str(), finalPath.c_str()) != 0)
    {
      fail("rename-failed", "could not rename to " + finalPath);
      return;
    }

    std::string line = "file=" + quoted(finalPath) +
                       " requested=" + requested +
                       " actual=" + QString::number(actualDuration, 'f', 1).toStdString() + "s" +
                       " source=" + QString::number(trim.sourceDuration, 'f', 1).toStdString() + "s" +
                       " cut_at=" + QString::number(trim.cutAt, 'f', 1).toStdString() + "s";

    if (!removeFileWithRetry(job.sourcePath))
    {
      // The clip is correct, but the untrimmed original is still sitting next to
      // it and the user will notice. Say so rather than reporting a clean success.
      Logger::warning("Could not delete the original file: %s", job.sourcePath.c_str());
      line += " original-not-deleted=" + quoted(job.sourcePath);
    }

    line += " elapsed=" + QString::number(elapsedSeconds(), 'f', 1).toStdString() + "s";

    reportVerdict("ok", line,
                  QString(obs_module_text("StatusTrimSuccess"))
                      .arg(formatDurationValue(job.duration)),
                  false);
  }

  bool ReplayBufferManager::verifyTrimmedOutput(const std::string &outputPath, int duration,
                                                double sourceDuration, double *actualDuration,
                                                std::string *reason)
  {
    const double actual = VideoTrimmer::getVideoDuration(outputPath);
    *actualDuration = actual;

    if (actual <= 0.0)
    {
      *reason = "output-unreadable";
      return false;
    }

    // A buffer holding less than the requested duration legitimately yields a
    // shorter clip, so measure against whichever is smaller.
    const double expected = (sourceDuration > 0.0)
                                ? std::min(static_cast<double>(duration), sourceDuration)
                                : static_cast<double>(duration);

    if (actual < expected * Config::TRIM_MIN_DURATION_RATIO)
    {
      *reason = "output-too-short";
      return false;
    }

    // Keyframe alignment only ever makes a clip longer, and by at most one GOP,
    // so the bar here is deliberately generous. Tripping it means the cut point
    // collapsed and the "trim" is effectively a copy of the whole buffer.
    if (actual > expected * Config::TRIM_MAX_DURATION_RATIO &&
        actual > expected + Config::TRIM_MAX_DURATION_SLACK_SECONDS)
    {
      *reason = "output-too-long";
      return false;
    }

    return true;
  }

  //=============================================================================
  // HELPER METHODS
  //=============================================================================

  std::string ReplayBufferManager::getTrimmedOutputPath(const std::string &sourcePath)
  {
    std::string stem;
    std::string extension;
    splitExtension(sourcePath, &stem, &extension);
    return stem + Config::TRIM_OUTPUT_SUFFIX + extension;
  }

  std::string ReplayBufferManager::getPartialOutputPath(const std::string &sourcePath)
  {
    std::string stem;
    std::string extension;
    splitExtension(sourcePath, &stem, &extension);
    // Keep the original extension last so libavformat still picks the right muxer
    return stem + Config::TRIM_PARTIAL_SUFFIX + extension;
  }

  bool ReplayBufferManager::removeFileWithRetry(const std::string &path)
  {
    if (!os_file_exists(path.c_str()))
    {
      return true;
    }

    int delayMs = Config::TRIM_UNLINK_RETRY_DELAY_MS;

    for (int attempt = 1; attempt <= Config::TRIM_UNLINK_RETRY_COUNT; attempt++)
    {
      if (os_unlink(path.c_str()) == 0 || !os_file_exists(path.c_str()))
      {
        return true;
      }

      if (attempt < Config::TRIM_UNLINK_RETRY_COUNT)
      {
        os_sleep_ms(delayMs);
        delayMs *= 2;
      }
    }

    return !os_file_exists(path.c_str());
  }

  void ReplayBufferManager::reportVerdict(const char *outcome, const std::string &detail,
                                          const QString &statusMessage, bool isFailure)
  {
    if (isFailure)
    {
      Logger::error("TRIM VERDICT: %s %s", outcome, detail.c_str());
    }
    else
    {
      Logger::info("TRIM VERDICT: %s %s", outcome, detail.c_str());
    }

    if (!statusMessage.isEmpty())
    {
      StatusReporter::showMessage(statusMessage,
                                  isFailure ? StatusReporter::FAILURE_TIMEOUT_MS
                                            : StatusReporter::DEFAULT_TIMEOUT_MS);
    }
  }

} // namespace ReplayBufferPro

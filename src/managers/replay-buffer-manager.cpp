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
    worker = std::thread([this]() { workerLoop(); });
  }

  ReplayBufferManager::~ReplayBufferManager()
  {
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

    enqueueRequest(duration);
    obs_frontend_replay_buffer_save();
    return true;
  }

  bool ReplayBufferManager::saveFullBuffer(QWidget *parent)
  {
    if (obs_frontend_replay_buffer_active())
    {
      // Queue an explicit do-not-trim marker rather than saving with nothing
      // pending, so this save consumes its own slot instead of inheriting a
      // duration left over from an earlier request OBS never honored.
      enqueueRequest(0);
      obs_frontend_replay_buffer_save();
      return true;
    }
    else if (parent)
    {
      QMessageBox::warning(parent, obs_module_text("Error"),
                           obs_module_text("ReplayBufferNotActive"));
    }
    return false;
  }

  //=============================================================================
  // REQUEST TRACKING
  //=============================================================================

  void ReplayBufferManager::expireStaleRequestsLocked(uint64_t now)
  {
    const uint64_t timeoutNs = static_cast<uint64_t>(Config::TRIM_REQUEST_TIMEOUT_MS) * 1000000ULL;

    while (!pendingSaves.empty() && now - pendingSaves.front().requestedAtNs > timeoutNs)
    {
      Logger::warning("Discarding save request for %ds that OBS never completed",
                      pendingSaves.front().duration);
      pendingSaves.pop_front();
    }
  }

  void ReplayBufferManager::enqueueRequest(int duration)
  {
    const uint64_t now = os_gettime_ns();
    const uint64_t coalesceNs = static_cast<uint64_t>(Config::TRIM_REQUEST_COALESCE_MS) * 1000000ULL;

    std::lock_guard<std::mutex> lock(pendingMutex);

    expireStaleRequestsLocked(now);

    // OBS tracks a single pending save timestamp, so two requests landing inside
    // one frame interval produce only one file. Collapsing them here keeps the
    // queue from running permanently one ahead of the saved events.
    if (!pendingSaves.empty() && now - pendingSaves.back().requestedAtNs < coalesceNs)
    {
      Logger::info("Coalescing save request for %ds into the previous one for %ds",
                   duration, pendingSaves.back().duration);
      pendingSaves.back().duration = duration;
      pendingSaves.back().requestedAtNs = now;
      return;
    }

    pendingSaves.push_back(PendingSave{duration, now});
  }

  std::optional<ReplayBufferManager::PendingSave> ReplayBufferManager::takeNextPendingSave()
  {
    const uint64_t now = os_gettime_ns();

    std::lock_guard<std::mutex> lock(pendingMutex);

    expireStaleRequestsLocked(now);

    if (pendingSaves.empty())
    {
      return std::nullopt;
    }

    PendingSave next = pendingSaves.front();
    pendingSaves.pop_front();
    return next;
  }

  //=============================================================================
  // SAVE COMPLETION
  //=============================================================================

  void ReplayBufferManager::handleSaveCompleted(const std::string &savedPath)
  {
    std::optional<PendingSave> pending = takeNextPendingSave();

    if (!pending.has_value())
    {
      // OBS's own Save Replay hotkey, the tray item and obs-websocket all reach
      // here. Those saves are not ours to trim, but saying so explicitly is what
      // turns "my clip wasn't trimmed" into an answerable question.
      reportVerdict("skipped",
                    "reason=no-pending-request file=" + quoted(savedPath),
                    QString(), false);
      return;
    }

    if (pending->duration <= 0)
    {
      reportVerdict("skipped",
                    "reason=save-full-buffer file=" + quoted(savedPath),
                    QString(), false);
      return;
    }

    if (savedPath.empty())
    {
      reportVerdict("failed",
                    "reason=no-saved-path requested=" + std::to_string(pending->duration) + "s",
                    QString(obs_module_text("StatusTrimFailed")).arg("no saved path"),
                    true);
      return;
    }

    // AutoRemux runs against this same file the instant OBS fires the saved
    // event, so note it up front when a trim later loses a race for the file.
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
        return;
      }
      jobQueue.push_back(TrimJob{savedPath, pending->duration});
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

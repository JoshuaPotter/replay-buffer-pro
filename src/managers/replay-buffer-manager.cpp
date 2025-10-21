/**
 * @file replay-buffer-manager.cpp
 * @brief Implementation of replay buffer management for the Replay Buffer Pro plugin
 */

#include "managers/replay-buffer-manager.hpp"
#include "managers/settings-manager.hpp"
#include "utils/logger.hpp"
#include "utils/video-trimmer.hpp"

// OBS includes
#include <util/platform.h>

// Qt includes
#include <QMessageBox>
#include <QString>

namespace ReplayBufferPro
{
  //=============================================================================
  // CONSTRUCTORS & DESTRUCTOR
  //=============================================================================

  ReplayBufferManager::ReplayBufferManager(QObject *parent)
      : QObject(parent), pendingSaveDuration(0)
  {
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

    pendingSaveDuration = duration; // Store the duration for the save completion handler
    obs_frontend_replay_buffer_save();
    return true;
  }

  bool ReplayBufferManager::saveFullBuffer(QWidget *parent)
  {
    if (obs_frontend_replay_buffer_active())
    {
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

  void ReplayBufferManager::setPendingSaveDuration(int duration)
  {
    pendingSaveDuration = duration;
  }

  int ReplayBufferManager::getPendingSaveDuration() const
  {
    return pendingSaveDuration;
  }

  void ReplayBufferManager::clearPendingSaveDuration()
  {
    pendingSaveDuration = 0;
  }

  //=============================================================================
  // REPLAY PROCESSING
  //=============================================================================

  std::string ReplayBufferManager::getTrimmedOutputPath(const char *sourcePath)
  {
    std::string path(sourcePath);
    size_t dot = path.find_last_of('.');
    if (dot != std::string::npos)
    {
      path.insert(dot, "_trimmed");
    }
    else
    {
      path += "_trimmed";
    }

    return path;
  }


  void ReplayBufferManager::trimReplayBuffer(const char *sourcePath, int duration)
  {
    try
    {
      Logger::info("Trimming replay buffer save to %d seconds", duration);

      std::string outputPath = getTrimmedOutputPath(sourcePath);

      // Use libavformat instead of external FFmpeg binary
      if (!VideoTrimmer::trimToLastSeconds(sourcePath, outputPath, duration))
      {
        throw std::runtime_error("Video trimming failed");
      }

      // Delete the original source file
      os_unlink(sourcePath);

      Logger::info("Successfully trimmed replay buffer to last %d seconds", duration);
    }
    catch (const std::exception &e)
    {
      Logger::error("Failed to trim replay: %s", e.what());
      QMessageBox::warning(nullptr, obs_module_text("Error"),
                           QString(obs_module_text("FailedToTrimReplay")).arg(e.what()));
    }
  }

} // namespace ReplayBufferPro
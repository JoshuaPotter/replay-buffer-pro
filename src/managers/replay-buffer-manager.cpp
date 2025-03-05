/**
 * @file replay-buffer-manager.cpp
 * @brief Implementation of replay buffer management for the Replay Buffer Pro plugin
 */

#include "managers/replay-buffer-manager.hpp"
#include "managers/settings-manager.hpp"
#include "utils/logger.hpp"

// OBS includes
#include <util/platform.h>

// Windows includes
#ifdef _WIN32
#include <windows.h>
#endif

// Qt includes
#include <QMessageBox>
#include <QString>

// STL includes
#include <sstream>

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

  bool ReplayBufferManager::executeFFmpegCommand(const std::string &command)
  {
#ifdef _WIN32
    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    // Create process
    if (!CreateProcessA(nullptr, (LPSTR)command.c_str(),
                        nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
    {
      return false;
    }

    // Wait for completion
    WaitForSingleObject(pi.hProcess, INFINITE);

    // Get exit code
    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    // Clean up
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return exitCode == 0;
#else
    return system(command.c_str()) == 0;
#endif
  }

  void ReplayBufferManager::trimReplayBuffer(const char *sourcePath, int duration)
  {
    try
    {
      Logger::info("Trimming replay buffer save to %d seconds", duration);

      // Get path to bundled FFmpeg
      char *ffmpegPath = obs_module_file("ffmpeg.exe");
      if (!ffmpegPath)
      {
        throw std::runtime_error("Could not locate bundled FFmpeg");
      }

      std::string outputPath = getTrimmedOutputPath(sourcePath);

      // Build FFmpeg command to get last N seconds
      // -sseof -N seeks to N seconds before the end of the file
      std::stringstream cmd;
      cmd << "\"" << ffmpegPath << "\" -y " // -y to overwrite output file
          << "-sseof -" << duration << " "  // Seek to duration seconds from end
          << "-i \"" << sourcePath << "\" "
          << "-c copy " // Copy streams without re-encoding
          << "\"" << outputPath << "\"";

      bfree(ffmpegPath);

      // Execute FFmpeg
      if (!executeFFmpegCommand(cmd.str()))
      {
        throw std::runtime_error("FFmpeg command failed");
      }

      // Delete the original source file
      os_unlink(sourcePath);

      Logger::info("Successfully trimmed replay to last %d seconds", duration);
    }
    catch (const std::exception &e)
    {
      Logger::error("Failed to trim replay: %s", e.what());
      QMessageBox::warning(nullptr, obs_module_text("Error"),
                           QString(obs_module_text("FailedToTrimReplay")).arg(e.what()));
    }
  }

} // namespace ReplayBufferPro
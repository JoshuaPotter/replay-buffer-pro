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
#include <string>
#include <stdexcept>

// Qt includes
#include <QObject>
#include <QMessageBox>

namespace ReplayBufferPro
{
  /**
   * @brief Manages replay buffer operations including saving and trimming
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
     * @brief Destructor
     */
    ~ReplayBufferManager() = default;

    //=========================================================================
    // REPLAY BUFFER OPERATIONS
    //=========================================================================
    /**
     * @brief Saves the replay buffer and set the duration for the pending trimming operation after save completes
     * @param duration Seconds to save
     * @param parent Parent widget for error messages
     * @return Success status
     */
    bool saveSegment(int duration, QWidget *parent = nullptr);

    /**
     * @brief Saves the entire replay buffer
     * @param parent Parent widget for error messages
     * @return Success status
     */
    bool saveFullBuffer(QWidget *parent = nullptr);

    /**
     * @brief Sets the pending save duration
     * @param duration Duration in seconds
     */
    void setPendingSaveDuration(int duration);

    /**
     * @brief Gets the pending save duration
     * @return Duration in seconds
     */
    int getPendingSaveDuration() const;

    /**
     * @brief Clears the pending save duration
     */
    void clearPendingSaveDuration();

    /**
     * @brief Trims a replay buffer file, called after save completes
     * @param sourcePath Source file path
     * @param duration Duration in seconds
     */
    void trimReplayBuffer(const char *sourcePath, int duration);

  private:
    //=========================================================================
    // MEMBER VARIABLES
    //=========================================================================
    int pendingSaveDuration; ///< Duration to save when buffer save completes

    //=========================================================================
    // HELPER METHODS
    //=========================================================================
    /**
     * @brief Gets output path for trimmed file
     * @param sourcePath Original file path
     * @return Trimmed file path
     */
    std::string getTrimmedOutputPath(const char *sourcePath);

    /**
     * @brief Executes an FFmpeg command
     * @param command Command to execute
     * @return Success status
     */
    bool executeFFmpegCommand(const std::string &command);
  };

} // namespace ReplayBufferPro
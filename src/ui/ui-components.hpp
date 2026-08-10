/**
 * @file ui-components.hpp
 * @brief UI components for the Replay Buffer Pro plugin
 * @author Joshua Potter
 * @copyright GPL v2 or later
 *
 * This file defines the UI components used by the Replay Buffer Pro plugin.
 */

#pragma once

// Qt includes
#include <QWidget>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QTimer>
#include <QPaintEvent>

// STL includes
#include <vector>
#include <functional>

// Project includes
#include "config/config.hpp"

namespace ReplayBufferPro
{
  /**
   * @class UIComponents
   * @brief Manages UI components for the Replay Buffer Pro plugin
   *
   * This class creates and manages the UI components used by the plugin,
   * including the buffer length spinbox and save buttons.
   */
  class UIComponents
  {
  public:
    //=========================================================================
    // CONSTRUCTORS & DESTRUCTOR
    //=========================================================================
    /**
     * @brief Creates UI components
     * @param parent Parent widget for UI components
     * @param saveSegmentCallback Callback for save segment button clicks
     * @param saveFullBufferCallback Callback for save full buffer button clicks
     */
    UIComponents(QWidget *parent,
                 std::function<void(int)> saveSegmentCallback,
                 std::function<void()> saveFullBufferCallback,
                 std::function<void()> customizeSaveButtonsCallback);

    /**
     * @brief Destructor
     */
    ~UIComponents() = default;

    //=========================================================================
    // GETTERS
    //=========================================================================
    /**
     * @brief Gets the buffer length text input
     * @return Pointer to the buffer length text input
     */
    QSpinBox *getSecondsEdit() const { return secondsEdit; }

    /**
     * @brief Gets the save buttons
     * @return Vector of save button pointers
     */
    const std::vector<QPushButton *> &getSaveButtons() const { return saveButtons; }

    /**
     * @brief Gets the save button durations
     * @return Vector of save button durations
     */
    const std::vector<int> &getSaveButtonDurations() const { return saveButtonDurations; }

    /**
     * @brief Gets the save full buffer button
     * @return Pointer to the save full buffer button
     */
    QPushButton *getSaveFullBufferBtn() const { return saveFullBufferBtn; }

    /**
     * @brief Gets the customize save buttons button
     * @return Pointer to the customize button
     */
    QPushButton *getCustomizeSaveButtonsBtn() const { return customizeSaveButtonsBtn; }

    /**
     * @brief Gets the buffer length debounce timer
     * @return Pointer to the buffer length debounce timer
     */
    QTimer *getBufferLengthDebounceTimer() const { return bufferLengthDebounceTimer; }

    //=========================================================================
    // UI CREATION
    //=========================================================================
    /**
     * @brief Creates the main UI layout
     * @return The main UI layout
     */
    QWidget *createUI();

    //=========================================================================
    // UI STATE MANAGEMENT
    //=========================================================================
    /**
     * @brief Updates UI components with new buffer length
     * @param seconds New buffer length in seconds
     */
    void updateBufferLengthValue(int seconds);

    /**
     * @brief Updates UI state based on replay buffer activity
     * @param isActive Whether the replay buffer is active
     */
    void updateBufferLengthState(bool isActive);

    /**
     * @brief Enables/disables save buttons based on buffer length
     * @param bufferLength Current buffer length in seconds
     */
    void toggleSaveButtons(int bufferLength);

    /**
     * @brief Updates save button durations and labels
     * @param durations Updated durations in seconds
     */
    void setSaveButtonDurations(const std::vector<int> &durations);

  private:
    //=========================================================================
    // UI COMPONENTS
    //=========================================================================
    QSpinBox *secondsEdit;                 ///< Buffer length control (1s to 6h)
    QPushButton *saveFullBufferBtn;         ///< Full buffer save trigger
    QPushButton *customizeSaveButtonsBtn;   ///< Customize save buttons trigger
    std::vector<QPushButton *> saveButtons; ///< Duration-specific save buttons
    QTimer *bufferLengthDebounceTimer;      ///< Prevents rapid setting updates
    std::vector<int> saveButtonDurations;   ///< Durations for save buttons

    //=========================================================================
    // CALLBACKS
    //=========================================================================
    std::function<void(int)> onSaveSegment; ///< Callback for save segment button clicks
    std::function<void()> onSaveFullBuffer; ///< Callback for save full buffer button clicks
    std::function<void()> onCustomizeSaveButtons; ///< Callback for customizing save buttons

    //=========================================================================
    // INITIALIZATION
    //=========================================================================
    /**
     * @brief Creates save duration buttons in a grid layout
     * @param layout Parent layout for the buttons
     */
    void initSaveButtons(QHBoxLayout *layout);

    void updateSaveButtonLabels();
    std::vector<int> getDefaultSaveButtonDurations() const;

    bool isBufferActive = false;  // Track buffer active state
  };
} // namespace ReplayBufferPro

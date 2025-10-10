/**
 * @file plugin.hpp
 * @brief Header file for the Replay Buffer Pro plugin for OBS Studio
 * @author Joshua Potter
 * @copyright GPL v2 or later
 *
 * This file defines the Plugin class which provides enhanced replay buffer
 * functionality for OBS Studio. The plugin adds a dockable widget that allows users
 * to control the replay buffer length and save segments of varying durations.
 *
 * Key features:
 * - Adjustable buffer length (10 seconds to 6 hours)
 * - Quick-save buttons for predefined durations
 * - Full buffer save capability
 * - Automatic UI state management based on buffer status
 * - Persistent dock position and settings
 */

#pragma once

// Core OBS includes
#include <obs.h>              // Core OBS API
#include <obs-module.h>       // Plugin module functions
#include <obs-frontend-api.h> // Frontend API functions

// Qt includes
#include <QWidget>
#include <QTimer>

// Local includes
#include "ui/ui-components.hpp"
#include "managers/settings-manager.hpp"
#include "managers/replay-buffer-manager.hpp"
#include "managers/hotkey-manager.hpp"

namespace ReplayBufferPro
{
  /**
   * @brief Main plugin class providing enhanced replay buffer controls
   *
   * Features:
   * - Adjustable buffer length (10 seconds to 6 hours)
   * - Quick-save buttons for predefined durations
   * - Full buffer save capability
   * - Automatic UI state management based on buffer status
   * - Persistent dock position and settings
   */
  class Plugin : public QWidget
  {
    Q_OBJECT

  public:
    //=========================================================================
    // CONSTRUCTORS & DESTRUCTOR
    //=========================================================================
    /**
     * @brief Creates a standalone dockable widget
     * @param parent Optional parent widget for memory management
     * 
     * Creates a floating/dockable widget that can be added to any Qt window.
     * Initializes all UI components and sets up event handling:
     * - Creates UI components with save callbacks
     * - Initializes signal connections
     * - Loads saved buffer length
     * - Sets up settings monitoring timer
     */
    explicit Plugin(QWidget *parent = nullptr);

    /**
     * @brief Cleans up resources and removes OBS event callbacks
     * 
     * Stops settings monitoring timer, removes OBS event callbacks,
     * and lets Qt handle component cleanup through parent-child relationship.
     */
    ~Plugin();

  private slots:
    //=========================================================================
    // EVENT HANDLERS
    //=========================================================================
    /**
     * @brief Updates UI and starts debounce timer when slider value changes
     * @param value New buffer length in seconds
     * 
     * Updates UI immediately and starts debounce timer for OBS settings update.
     * Prevents rapid OBS settings updates during slider movement.
     */
    void handleSliderChanged(int value);

    /**
     * @brief Updates OBS settings after slider movement ends
     * 
     * Called after slider movement stops and debounce period expires.
     * Updates OBS settings with the final slider value.
     * Shows error message if update fails.
     */
    void handleSliderFinished();

    /**
     * @brief Validates and applies manual buffer length input
     * 
     * Ensures input is within valid range (10s to 6h) and updates settings.
     * Reverts to previous value if input is invalid.
     * Shows error message if update fails.
     */
    void handleBufferLengthInput(int value);

    /**
     * @brief Updates UI state based on replay buffer activity
     * 
     * Synchronizes UI with replay buffer state:
     * - Enables/disables controls based on buffer activity
     * - Updates buffer length display when active
     */
    void updateBufferLengthUIState();

    /**
     * @brief Loads and applies saved buffer length from OBS settings
     * 
     * Retrieves and applies saved buffer length:
     * - Handles both Simple and Advanced output modes
     * - Falls back to default length (5m) if not set
     * - Updates UI with loaded value
     */
    void loadBufferLength();

    /**
     * @brief Handles replay buffer saved event
     * 
     * Handles the event when a replay buffer is saved.
     * Invokes the trim operation and clears the pending save duration, if needed.
     */
    void handleReplayBufferSaved();

  private:
    //=========================================================================
    // COMPONENT INSTANCES
    //=========================================================================
    UIComponents *ui;                   ///< UI components
    SettingsManager *settingsManager;   ///< Settings manager
    ReplayBufferManager *replayManager; ///< Replay buffer manager
    HotkeyManager *hotkeyManager;       ///< Hotkey manager
    QTimer *settingsMonitorTimer;       ///< Timer for monitoring OBS settings changes
    uint64_t lastKnownBufferLength;     ///< Last known buffer length from OBS settings

    //=========================================================================
    // INITIALIZATION
    //=========================================================================
    /**
     * @brief Sets up signal/slot connections
     * 
     * Sets up all event handling connections:
     * - Slider value changes (with debouncing)
     * - Text input validation
     * - Save button clicks
     */
    void initSignals();

    //=========================================================================
    // EVENT HANDLERS
    //=========================================================================
    /**
     * @brief Saves a specific duration from the replay buffer
     * @param duration Duration in seconds to save
     * 
     * Saves a specific duration from the replay buffer:
     * - Verifies buffer is active
     * - Checks if duration exceeds buffer length
     * - Shows appropriate error messages
     * - Triggers save if all checks pass
     */
    void handleSaveSegment(int duration);

    /**
     * @brief Triggers full buffer save if replay buffer is active
     * 
     * Triggers saving of entire replay buffer content.
     * Shows error if buffer is not active.
     */
    void handleSaveFullBuffer();

    /**
     * @brief Handles OBS frontend events for buffer state changes
     * @param event Event type from OBS
     * @param ptr Instance pointer for callback routing
     * 
     * Handles OBS events related to replay buffer state changes:
     * - Buffer starting/stopping: Updates UI state
     * - Buffer started/stopped: Updates UI and settings monitoring
     * - Buffer saved: Handles segment trimming if needed
     * Uses Qt's event system to safely update UI from any thread.
     */
    static void handleOBSEvent(enum obs_frontend_event event, void *ptr);
  };

} // namespace ReplayBufferPro
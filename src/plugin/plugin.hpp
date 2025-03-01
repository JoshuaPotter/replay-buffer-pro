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
#include <QDockWidget>
#include <QMainWindow>
#include <QTimer>

// Local includes
#include "ui/ui-components.hpp"
#include "managers/settings-manager.hpp"
#include "managers/dock-state-manager.hpp"
#include "managers/replay-buffer-manager.hpp"

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
  class Plugin : public QDockWidget
  {
    Q_OBJECT

  public:
    //=========================================================================
    // CONSTRUCTORS & DESTRUCTOR
    //=========================================================================
    /**
     * @brief Creates a standalone dockable widget
     * @param parent Optional parent widget for memory management
     */
    explicit Plugin(QWidget *parent = nullptr);

    /**
     * @brief Creates and docks widget to OBS main window
     * @param mainWindow The OBS main window to dock to
     */
    explicit Plugin(QMainWindow *mainWindow);

    /**
     * @brief Cleans up resources and removes OBS event callbacks
     */
    ~Plugin();

    /**
     * @brief Handles OBS frontend events for buffer state changes
     * @param event Event type from OBS
     * @param ptr Instance pointer for callback routing
     */
    static void handleOBSEvent(enum obs_frontend_event event, void *ptr);

  private slots:
    //=========================================================================
    // EVENT HANDLERS
    //=========================================================================
    /**
     * @brief Updates UI and starts debounce timer when slider value changes
     * @param value New buffer length in seconds
     */
    void handleSliderChanged(int value);

    /**
     * @brief Updates OBS settings after slider movement ends
     */
    void handleSliderFinished();

    /**
     * @brief Validates and applies manual buffer length input
     */
    void handleBufferLengthInput();

    /**
     * @brief Updates UI state based on replay buffer activity
     */
    void updateBufferLengthUIState();

    /**
     * @brief Loads and applies saved buffer length from OBS settings
     */
    void loadBufferLength();

  private:
    //=========================================================================
    // COMPONENT INSTANCES
    //=========================================================================
    UIComponents *ui;                   ///< UI components
    SettingsManager *settingsManager;   ///< Settings manager
    DockStateManager *dockStateManager; ///< Dock state manager
    ReplayBufferManager *replayManager; ///< Replay buffer manager
    QTimer *settingsMonitorTimer;       ///< Timer for monitoring OBS settings changes
    uint64_t lastKnownBufferLength;     ///< Last known buffer length from OBS settings

    //=========================================================================
    // INITIALIZATION
    //=========================================================================
    /**
     * @brief Sets up signal/slot connections
     */
    void initSignals();

    //=========================================================================
    // EVENT HANDLERS
    //=========================================================================
    /**
     * @brief Saves a specific duration from the replay buffer
     * @param duration Duration in seconds to save
     */
    void handleSaveSegment(int duration);

    /**
     * @brief Triggers full buffer save if replay buffer is active
     */
    void handleSaveFullBuffer();
  };

} // namespace ReplayBufferPro
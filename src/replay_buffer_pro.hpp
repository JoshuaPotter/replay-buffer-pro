/**
 * @file replay_buffer_pro.hpp
 * @brief Header file for the Replay Buffer Pro plugin for OBS Studio
 * @author Joshua Potter
 * @copyright GPL v2 or later
 *
 * This file defines the ReplayBufferPro class which provides enhanced replay buffer
 * functionality for OBS Studio. The plugin adds a dockable widget that allows users
 * to control the replay buffer length and save segments of varying durations.
 *
 * Key features:
 * - Adjustable replay buffer length via slider (10 seconds to 1 hour)
 * - Quick-save buttons for common durations (15s, 30s, 1m, etc.)
 * - Full buffer save functionality
 * - Real-time buffer state monitoring
 */

#pragma once

// Core OBS includes
#include <libobs/obs.h>              // Core OBS API
#include <libobs/obs-module.h>       // Plugin module functions
#include <frontend/api/obs-frontend-api.h> // Frontend API functions
#include <util/config-file.h>        // For config_* functions
#include <util/platform.h>           // For os_mkdirs and other platform functions

// Qt includes needed for class declaration
#include <QDockWidget>
#include <QWidget>
#include <QMainWindow>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QLineEdit>
#include <vector>

/**
 * @class ReplayBufferPro
 * @brief Main widget class for the Replay Buffer Pro plugin
 *
 * This class implements a dockable widget that provides an enhanced interface
 * for controlling OBS Studio's replay buffer functionality. It allows users to:
 * - Adjust the replay buffer length using a slider
 * - Save specific segments of the replay buffe
 *
 * The widget integrates with OBS Studio's main window.
 */
class ReplayBufferPro : public QDockWidget {
    Q_OBJECT

public:
    /**
     * @brief Constructs the ReplayBufferPro widget with a generic parent
     * @param parent The parent widget (optional)
     */
    explicit ReplayBufferPro(QWidget *parent = nullptr);

    /**
     * @brief Constructs the ReplayBufferPro widget and adds it to a main window
     * @param mainWindow The main window to dock this widget to
     * 
     * This constructor automatically adds the widget to the right dock area
     * of the specified main window.
     */
    explicit ReplayBufferPro(QMainWindow *mainWindow);

    /**
     * @brief Destructor - Cleans up resources and removes event callbacks
     */
    ~ReplayBufferPro();

    /**
     * @brief Static callback for OBS frontend events
     * @param event The OBS frontend event type
     * @param ptr Pointer to the ReplayBufferPro instance
     */
    static void OBSFrontendEvent(enum obs_frontend_event event, void *ptr);

private slots:
    /**
     * @brief Updates the slider state based on replay buffer activity
     * 
     * Disables the slider when replay buffer is active and updates
     * the current buffer length.
     */
    void updateSliderState();

    /**
     * @brief Saves the entire contents of the replay buffer
     * 
     * Shows a warning if the replay buffer is not active.
     */
    void saveFullBuffer();

    /**
     * @brief Saves a specific duration segment from the replay buffer
     * @param duration The duration in seconds to save
     * 
     * Shows a warning if the requested duration exceeds the current
     * buffer length or if the buffer is not active.
     */
    void saveReplaySegment(int duration);

    /**
     * @brief Handles text input from the seconds edit box
     */
    void handleTextInput();

private:
    // UI Elements
    QSlider *slider;              ///< Slider for adjusting buffer length
    QLineEdit *secondsEdit;       ///< Text box for buffer length
    QLabel *secondsLabel;         ///< Label showing "s" suffix
    QPushButton *saveFullBufferBtn; ///< Button to save entire buffer
    std::vector<QPushButton*> saveButtons; ///< Buttons for saving segments

    /**
     * @brief Initializes the UI components
     * 
     * Creates and arranges all UI elements including the slider,
     * labels, and save buttons.
     */
    void initializeUI();

    /**
     * @brief Loads the saved dock state
     * @param mainWindow The main window to dock to
     * 
     * Restores the dock widget's position and geometry from saved settings.
     * Defaults to left dock area if no saved state exists.
     */
    void loadDockState(QMainWindow *mainWindow);

    /**
     * @brief Saves the current dock state
     * 
     * Saves the current dock position and geometry to OBS settings.
     * Called automatically when the dock location changes.
     */
    void saveDockState();

    /**
     * @brief Initializes the save buttons for different durations
     * @param layout The layout to add the buttons to
     */
    void initializeSaveButtons(QHBoxLayout *layout);

    /**
     * @brief Connects all signals to their corresponding slots
     */
    void connectSignalsAndSlots();

    /**
     * @brief Loads the current buffer length from OBS settings
     * 
     * Falls back to default length if settings cannot be loaded.
     */
    void loadCurrentBufferLength();

    /**
     * @brief Sets the buffer length and updates UI
     * @param seconds The new buffer length in seconds
     */
    void setBufferLength(int seconds);

    /**
     * @brief Updates the enabled state of save buttons
     * @param bufferLength Current buffer length in seconds
     * 
     * Disables buttons for durations longer than the current buffer length.
     */
    void updateButtonStates(int bufferLength);

    /**
     * @brief Updates the replay buffer settings in OBS
     * @param seconds The new buffer length in seconds
     * 
     * Handles stopping and restarting the buffer if it's active,
     * and updates both OBS config and output settings.
     */
    void updateReplayBufferSettings(int seconds);
};
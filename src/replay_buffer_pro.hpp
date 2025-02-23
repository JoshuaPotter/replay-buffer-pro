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
#include <QTimer>

/**
 * @class ReplayBufferPro
 * @brief Main widget class for the Replay Buffer Pro plugin
 *
 * This class implements a dockable widget that provides an enhanced interface
 * for controlling OBS Studio's replay buffer functionality. It integrates with
 * OBS Studio's main window and provides real-time buffer control and monitoring.
 * 
 * Features:
 * - Adjustable buffer length (10 seconds to 6 hours)
 * - Quick-save buttons for predefined durations
 * - Full buffer save capability
 * - Automatic UI state management based on buffer status
 * - Persistent dock position and settings
 */
class ReplayBufferPro : public QDockWidget {
    Q_OBJECT

public:
    // Constructors & Destructor
    explicit ReplayBufferPro(QWidget *parent = nullptr);
    explicit ReplayBufferPro(QMainWindow *mainWindow);
    ~ReplayBufferPro();

    /**
     * @brief Static callback for OBS frontend events
     * @param event The type of event that occurred
     * @param ptr Pointer to the ReplayBufferPro instance
     */
    static void handleOBSEvent(enum obs_frontend_event event, void *ptr);

private slots:
    // Event Handlers
    /**
     * @brief Handles changes to the buffer length slider
     * @param value New slider value in seconds
     */
    void handleSliderChanged(int value);

    /**
     * @brief Handles when slider movement has stopped
     * Updates OBS settings after debounce period
     */
    void handleSliderFinished();

    /**
     * @brief Handles manual text input for buffer length
     */
    void handleBufferLengthInput();

    /**
     * @brief Handles saving full replay buffer
     */
    void handleSaveFullBuffer();

    /**
     * @brief Handles saving a segment of the replay buffer
     * @param duration Number of seconds to save
     */
    void handleSaveSegment(int duration);

private:
    // UI Components
    QSlider *slider;              ///< Slider for adjusting buffer length (10s to 6h)
    QLineEdit *secondsEdit;       ///< Text input for precise buffer length
    QLabel *secondsLabel;         ///< "s" suffix label for seconds
    QPushButton *saveFullBufferBtn; ///< Triggers full buffer save
    std::vector<QPushButton*> saveButtons; ///< Quick-save duration buttons
    QTimer *sliderDebounceTimer; ///< Prevents rapid settings updates

    // Initialization
    /**
     * @brief Initializes all UI components and layouts
     */
    void initUI();

    /**
     * @brief Initializes save duration buttons
     * @param layout The layout to add buttons to
     */
    void initSaveButtons(QHBoxLayout *layout);

    /**
     * @brief Initializes all signal/slot connections
     */
    void initSignals();

    // State Management
    /**
     * @brief Updates UI components with new buffer length
     * @param seconds New buffer length in seconds
     */
    void updateBufferLengthUI(int seconds);

    /**
     * @brief Updates OBS settings with new buffer length
     * @param seconds New buffer length in seconds
     */
    void updateOBSBufferLength(int seconds);

    /**
     * @brief Updates UI state based on buffer activity
     */
    void updateUIState();

    /**
     * @brief Enables/disables save buttons based on buffer length
     * @param bufferLength Current buffer length in seconds
     */
    void toggleSaveButtons(int bufferLength);

    // Persistence
    /**
     * @brief Loads buffer length from OBS settings
     */
    void loadBufferLength();

    /**
     * @brief Loads dock position and state
     * @param mainWindow The main window to dock to
     */
    void loadDockState(QMainWindow *mainWindow);

    /**
     * @brief Saves current dock position and state
     */
    void saveDockState();
};
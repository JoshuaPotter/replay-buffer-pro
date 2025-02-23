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
 * - Adjustable buffer length (10 seconds to 6 hours)
 * - Quick-save buttons for predefined durations
 * - Full buffer save capability
 * - Automatic UI state management based on buffer status
 * - Persistent dock position and settings
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

class QSlider;
class QLineEdit;
class QPushButton;
class QTimer;

/**
 * @brief Configuration constants for the plugin
 */
namespace Config {
    // Buffer length configuration
    constexpr int MIN_BUFFER_LENGTH = 10;      // 10 seconds minimum
    constexpr int MAX_BUFFER_LENGTH = 21600;   // 6 hours maximum
    constexpr int DEFAULT_BUFFER_LENGTH = 300;  // 5 minutes default
    
    // Configuration keys
    constexpr const char* REPLAY_BUFFER_LENGTH_KEY = "RecRBTime";
    constexpr const char* DOCK_AREA_KEY = "DockArea";
    constexpr const char* DOCK_GEOMETRY_KEY = "DockGeometry";
    
    // Timer intervals
    constexpr int SETTINGS_MONITOR_INTERVAL = 1000;  // 1 second
    constexpr int SLIDER_DEBOUNCE_INTERVAL = 500;    // 500 milliseconds
    
    // File paths
    constexpr const char* DOCK_STATE_FILENAME = "dock_state.json";
    constexpr const char* TEMP_FILE_SUFFIX = "tmp";
    constexpr const char* BACKUP_FILE_SUFFIX = "bak";
}

/**
 * @brief Utility class for standardized logging
 */
class Logger {
public:
    static void info(const char* format, ...) {
        va_list args;
        va_start(args, format);
        char buf[4096];
        vsnprintf(buf, sizeof(buf), format, args);
        blog(LOG_INFO, "[ReplayBufferPro] %s", buf);
        va_end(args);
    }

    static void error(const char* format, ...) {
        va_list args;
        va_start(args, format);
        char buf[4096];
        vsnprintf(buf, sizeof(buf), format, args);
        blog(LOG_ERROR, "[ReplayBufferPro] %s", buf);
        va_end(args);
    }
};

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
    //=========================================================================
    // CONSTRUCTORS & DESTRUCTOR
    //=========================================================================
    /**
     * @brief Creates a standalone dockable widget
     * @param parent Optional parent widget for memory management
     */
    explicit ReplayBufferPro(QWidget *parent = nullptr);

    /**
     * @brief Creates and docks widget to OBS main window
     * @param mainWindow The OBS main window to dock to
     */
    explicit ReplayBufferPro(QMainWindow *mainWindow);

    /**
     * @brief Cleans up resources and callbacks
     */
    ~ReplayBufferPro();

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
     * @brief Handles slider value changes with debouncing
     * @param value New buffer length in seconds
     */
    void handleSliderChanged(int value);

    /**
     * @brief Updates settings after slider movement ends
     */
    void handleSliderFinished();

    /**
     * @brief Validates and applies manual buffer length input
     */
    void handleBufferLengthInput();

    /**
     * @brief Saves entire replay buffer if active
     */
    void handleSaveFullBuffer();

    /**
     * @brief Saves specific duration from buffer if possible
     * @param duration Seconds to save from buffer
     */
    void handleSaveSegment(int duration);

    /**
     * @brief Loads buffer length from OBS settings
     * 
     * Retrieves and applies saved buffer length:
     * - Handles both Simple and Advanced output modes
     * - Falls back to default length (5m) if not set
     * - Updates UI with loaded value
     */
    void loadBufferLength();

    //=========================================================================
    // PERSISTENCE
    //=========================================================================
    /**
     * @brief Restores dock position and state
     * @param mainWindow Main window to dock to
     */
    void loadDockState(QMainWindow *mainWindow);

    /**
     * @brief Saves current dock position and state
     */
    void saveDockState();

    /**
     * @brief Updates UI based on buffer activity
     */
    void updateUIState();

private:
    //=========================================================================
    // UI COMPONENTS
    //=========================================================================
    QSlider *slider;              ///< Buffer length control (10s to 6h)
    QLineEdit *secondsEdit;       ///< Manual buffer length input
    QLabel *secondsLabel;         ///< Seconds unit label
    QPushButton *saveFullBufferBtn; ///< Full buffer save trigger
    std::vector<QPushButton*> saveButtons; ///< Duration-specific save buttons
    QTimer *sliderDebounceTimer; ///< Prevents rapid setting updates
    QTimer *settingsMonitorTimer; ///< Timer for monitoring OBS settings changes
    uint64_t lastKnownBufferLength; ///< Last known buffer length from OBS settings

    //=========================================================================
    // INITIALIZATION
    //=========================================================================
    /**
     * @brief Creates and arranges UI components
     */
    void initUI();

    /**
     * @brief Creates and configures save duration buttons
     * @param layout Parent layout for button grid
     */
    void initSaveButtons(QHBoxLayout *layout);

    /**
     * @brief Sets up signal/slot connections
     */
    void initSignals();

    //=========================================================================
    // STATE MANAGEMENT
    //=========================================================================
    /**
     * @brief Updates UI to reflect new buffer length
     * @param seconds New buffer length
     */
    void updateBufferLengthUI(int seconds);

    /**
     * @brief Updates OBS configuration with new length
     * @param seconds New buffer length
     */
    void updateOBSBufferLength(int seconds);

    /**
     * @brief Updates save button enabled states
     * @param bufferLength Current buffer length
     */
    void toggleSaveButtons(int bufferLength);
};
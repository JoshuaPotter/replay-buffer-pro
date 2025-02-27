/**
 * @file replay_buffer_pro.hpp
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
#include <util/config-file.h> // For config_* functions
#include <util/platform.h>    // For os_mkdirs and other platform functions

// Qt includes
#include <QDockWidget>
#include <QMainWindow>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QLineEdit>
#include <QTimer>

// STL includes
#include <vector>
#include <string>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#endif

// Local includes
#include "config.hpp"
#include "logger.hpp"

namespace ReplayBufferPro
{

    /**
     * @brief Structure defining configuration context
     */
    struct ConfigContext
    {
        config_t *config;
        const char *section;
    };

    /**
     * @class Plugin
     * @brief Main plugin class for the Replay Buffer Pro plugin
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
         * @brief Cleans up resources and callbacks
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
        void updateBufferLengthUIState();

    private:
        //=========================================================================
        // UI COMPONENTS
        //=========================================================================
        QSlider *slider;                        ///< Buffer length control (10s to 6h)
        QLineEdit *secondsEdit;                 ///< Manual buffer length input
        QLabel *secondsLabel;                   ///< Seconds unit label
        QPushButton *saveFullBufferBtn;         ///< Full buffer save trigger
        std::vector<QPushButton *> saveButtons; ///< Duration-specific save buttons
        QTimer *sliderDebounceTimer;            ///< Prevents rapid setting updates
        QTimer *settingsMonitorTimer;           ///< Timer for monitoring OBS settings changes
        uint64_t lastKnownBufferLength;         ///< Last known buffer length from OBS settings
        int pendingSaveDuration;                ///< Duration to save when buffer save completes

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
        void updateBufferLengthUIValue(int seconds);

        /**
         * @brief Updates OBS configuration with new length
         * @param seconds New buffer length
         */
        void updateBufferLengthSettings(int seconds);

        /**
         * @brief Updates save button enabled states
         * @param bufferLength Current buffer length
         */
        void toggleSaveButtons(int bufferLength);

        //=========================================================================
        // REPLAY BUFFER TRIMMING
        //=========================================================================
        /**
         * @brief Gets the output path for a trimmed replay buffer file
         * @param sourcePath Original replay buffer file path
         * @return Path for the trimmed file
         */
        std::string getTrimmedOutputPath(const char *sourcePath);

        /**
         * @brief Executes an FFmpeg command
         * @param command FFmpeg command to execute
         * @return true if command succeeded, false otherwise
         */
        bool executeFFmpegCommand(const std::string &command);

        /**
         * @brief Trims a replay buffer file to specified duration
         * @param sourcePath Path to the replay buffer file
         * @param duration Duration in seconds to trim to
         */
        void trimReplayBuffer(const char *sourcePath, int duration);

        /**
         * @brief Gets the OBS configuration context
         * @return ConfigContext struct with config and section
         */
        ConfigContext getConfigContext();
    };

} // namespace ReplayBufferPro
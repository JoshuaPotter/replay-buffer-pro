/**
 * @file dock_state_manager.hpp
 * @brief Dock state management for the Replay Buffer Pro plugin
 * @author Joshua Potter
 * @copyright GPL v2 or later
 *
 * This file defines the DockStateManager class which handles dock position persistence.
 */

#pragma once

// Qt includes
#include <QDockWidget>
#include <QMainWindow>

// OBS includes
#include <obs-module.h>

namespace ReplayBufferPro
{
    /**
     * @class DockStateManager
     * @brief Manages dock state for the Replay Buffer Pro plugin
     *
     * This class handles saving and loading dock position and state.
     */
    class DockStateManager
    {
    public:
        //=========================================================================
        // CONSTRUCTORS & DESTRUCTOR
        //=========================================================================
        /**
         * @brief Creates a dock state manager
         * @param dockWidget The dock widget to manage
         */
        explicit DockStateManager(QDockWidget *dockWidget);

        /**
         * @brief Destructor
         */
        ~DockStateManager() = default;

        //=========================================================================
        // DOCK STATE MANAGEMENT
        //=========================================================================
        /**
         * @brief Restores saved dock position and state
         * @param mainWindow Main window to dock to
         */
        void loadDockState(QMainWindow *mainWindow);

        /**
         * @brief Persists current dock position and state
         */
        void saveDockState();

    private:
        //=========================================================================
        // MEMBER VARIABLES
        //=========================================================================
        QDockWidget *dockWidget; ///< The dock widget to manage
    };

} // namespace ReplayBufferPro 
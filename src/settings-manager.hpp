/**
 * @file settings-manager.hpp
 * @brief Settings management for the Replay Buffer Pro plugin
 * @author Joshua Potter
 * @copyright GPL v2 or later
 *
 * This file defines the SettingsManager class which handles OBS settings interactions.
 */

#pragma once

// OBS includes
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>

// STL includes
#include <string>
#include <stdexcept>

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
     * @class SettingsManager
     * @brief Manages OBS settings for the Replay Buffer Pro plugin
     *
     * This class handles interactions with OBS settings, including loading and
     * updating buffer length settings.
     */
    class SettingsManager
    {
    public:
        //=========================================================================
        // CONSTRUCTORS & DESTRUCTOR
        //=========================================================================
        /**
         * @brief Creates a settings manager
         */
        SettingsManager() = default;

        /**
         * @brief Destructor
         */
        ~SettingsManager() = default;

        //=========================================================================
        // SETTINGS MANAGEMENT
        //=========================================================================
        /**
         * @brief Gets OBS configuration context based on output mode
         * @return Configuration context containing config pointer and section name
         * @throws std::runtime_error If config cannot be accessed
         */
        ConfigContext getConfigContext();

        /**
         * @brief Updates OBS settings with new buffer length
         * @param seconds New buffer length in seconds
         * @throws std::runtime_error If settings update fails
         */
        void updateBufferLengthSettings(int seconds);

        /**
         * @brief Loads buffer length from OBS settings
         * @return Current buffer length in seconds
         */
        int loadBufferLength();

        /**
         * @brief Gets the current buffer length from OBS settings
         * @return Current buffer length in seconds
         */
        int getCurrentBufferLength();
    };

} // namespace ReplayBufferPro 
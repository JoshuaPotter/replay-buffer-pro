/**
 * @file hotkey-manager.hpp
 * @brief Manages hotkeys for the Replay Buffer Pro plugin
 * @author Joshua Potter
 * @copyright GPL v2 or later
 */

#pragma once

// OBS includes
#include <obs-module.h>
#include <obs-frontend-api.h>

// STL includes
#include <functional>
#include <string>

// Local includes
#include "ui/ui-components.hpp"

namespace ReplayBufferPro
{
  /**
   * @brief Manages hotkey registration, saving, and loading for the plugin
   */
  class HotkeyManager
  {
  public:
    //=========================================================================
    // CONSTRUCTORS & DESTRUCTOR
    //=========================================================================
    /**
     * @brief Constructor
     * @param saveSegmentCallback Callback for save segment hotkeys
     */
    HotkeyManager(
        std::function<void(int)> saveSegmentCallback
    );

    /**
     * @brief Destructor
     * 
     * Unregisters all hotkeys and saves settings
     */
    ~HotkeyManager();

    //=========================================================================
    // HOTKEY MANAGEMENT
    //=========================================================================
    /**
     * @brief Registers all hotkeys with OBS
     * 
     * Creates hotkeys for each save duration button.
     * Users can assign key combinations to these hotkeys in OBS settings.
     */
    void registerHotkeys();

    /**
     * @brief Saves hotkey settings to OBS config
     * 
     * Saves the current hotkey assignments to the OBS configuration.
     */
    void saveHotkeySettings();

    /**
     * @brief Loads hotkey settings from OBS config
     * 
     * Loads previously saved hotkey assignments from the OBS configuration.
     */
    void loadHotkeySettings();

  private:
    //=========================================================================
    // MEMBER VARIABLES
    //=========================================================================
    obs_hotkey_id saveHotkeys[SAVE_BUTTON_COUNT]; ///< Array of hotkey IDs for each save duration
    std::function<void(int)> onSaveSegment;       ///< Callback for save segment hotkeys
  };

} // namespace ReplayBufferPro 
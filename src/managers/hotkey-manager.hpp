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
#include <vector>

// Local includes
#include "ui/ui-components.hpp"
#include "config/config.hpp"

namespace ReplayBufferPro
{
  /**
   * @brief Manages hotkey registration for the Replay Buffer Pro plugin
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
         std::function<void(int)> saveSegmentCallback,
         const std::vector<int> &saveButtonDurations
    );

    /**
     * @brief Destructor
     */
    ~HotkeyManager() = default;

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
     * @brief Saves current hotkey bindings to disk
     * 
     * Persists all hotkey bindings to the plugin's config file
     * so they can be restored in future sessions.
     */
    void saveHotkeySettings();

    /**
     * @brief Updates durations used by hotkeys and refreshes descriptions
     * @param saveButtonDurations Updated durations for each save button
     */
    void setSaveButtonDurations(const std::vector<int> &saveButtonDurations);

  private:
    //=========================================================================
    // MEMBER VARIABLES
    //=========================================================================
    obs_hotkey_id saveHotkeys[Config::SAVE_BUTTON_COUNT]; ///< Array of hotkey IDs for each save duration
    std::function<void(int)> onSaveSegment;       ///< Callback for save segment hotkeys
    std::vector<int> saveButtonDurations;         ///< Current durations for save buttons
    bool hotkeysRegistered = false;

    //=========================================================================
    // PRIVATE METHODS
    //=========================================================================

    /**
     * @brief Loads saved hotkey bindings from disk
     * 
     * Restores previously saved hotkey bindings from the plugin's config file.
     */
    void loadHotkeySettings();

    int getDurationForIndex(size_t index) const;
    void updateHotkeyDescriptions();
  };

} // namespace ReplayBufferPro 

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
#include "managers/save-button-settings.hpp"

namespace ReplayBufferPro
{
  /**
   * @brief Manages hotkey registration for the Replay Buffer Pro plugin
   *
   * One hotkey per save button (only as many as there are buttons) plus one
   * for Save Replay Buffer. Bindings of buttons that are removed stay in
   * hotkey_bindings.json, so raising the button count again restores them.
   */
  class HotkeyManager
  {
  public:
    /**
     * @brief Target index for the Save Replay Buffer hotkey in getBindings/setBinding
     */
    static constexpr size_t FULL_BUFFER = Config::MAX_SAVE_BUTTON_COUNT;

    //=========================================================================
    // CONSTRUCTORS & DESTRUCTOR
    //=========================================================================
    /**
     * @brief Constructor
     * @param saveSegmentCallback Callback for save segment hotkeys
     * @param saveFullBufferCallback Callback for the Save Replay Buffer hotkey
     * @param buttons Current save buttons
     */
     HotkeyManager(
         std::function<void(int)> saveSegmentCallback,
         std::function<void()> saveFullBufferCallback,
         const std::vector<SaveButton> &buttons
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
     * Creates a hotkey for each save button and one for Save Replay Buffer.
     * Users can assign key combinations to these hotkeys in OBS settings.
     */
    void registerHotkeys();

    /**
     * @brief Saves current hotkey bindings to disk
     *
     * Persists the bindings of every registered hotkey to the plugin's config
     * file, keeping the saved bindings of buttons that are not registered.
     */
    void saveHotkeySettings();

    /**
     * @brief Matches the hotkeys to the save buttons
     * @param buttons Updated save buttons
     *
     * Registers or unregisters hotkeys when the button count changes and
     * refreshes their names.
     */
    void setButtons(const std::vector<SaveButton> &buttons);

    /**
     * @brief Gets the key combinations bound to a hotkey
     * @param target Save button index, or FULL_BUFFER
     * @return Every binding, in the order OBS holds them (empty if unbound). For a
     *         button that is not registered, the bindings saved in the config file.
     */
    std::vector<obs_key_combination_t> getBindings(size_t target) const;

    /**
     * @brief Replaces a hotkey's bindings with one key combination
     * @param target Save button index, or FULL_BUFFER
     * @param combination New binding, or nullptr to remove every binding
     */
    void setBinding(size_t target, const obs_key_combination_t *combination);

  private:
    //=========================================================================
    // MEMBER VARIABLES
    //=========================================================================
    obs_hotkey_id saveHotkeys[Config::MAX_SAVE_BUTTON_COUNT]; ///< Hotkey ID of each save button slot
    obs_hotkey_id saveFullHotkey = OBS_INVALID_HOTKEY_ID;     ///< Save Replay Buffer hotkey
    std::function<void(int)> onSaveSegment;       ///< Callback for save segment hotkeys
    std::function<void()> onSaveFullBuffer;       ///< Callback for the Save Replay Buffer hotkey
    std::vector<SaveButton> buttons;              ///< Current save buttons
    bool hotkeysRegistered = false;

    //=========================================================================
    // PRIVATE METHODS
    //=========================================================================
    obs_hotkey_id hotkeyId(size_t target) const;
    void registerButtonHotkey(size_t index);
    void updateHotkeyDescriptions();

    /**
     * @brief Loads saved bindings from disk into the given hotkeys
     * @param targets Save button indexes and/or FULL_BUFFER
     */
    void loadHotkeySettings(const std::vector<size_t> &targets);
  };

} // namespace ReplayBufferPro

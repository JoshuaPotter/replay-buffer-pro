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

// Qt includes
#include <QObject>

// STL includes
#include <functional>
#include <string>
#include <vector>

// Local includes
#include "ui/ui-components.hpp"
#include "config/config.hpp"
#include "managers/plugin-properties.hpp"

namespace ReplayBufferPro
{
  /**
   * @brief Manages hotkey registration for the Replay Buffer Pro plugin
   *
   * One hotkey per save button (only as many as there are buttons). Bindings
   * of buttons that are removed stay in hotkey_bindings.json, so raising the
   * button count again restores them.
   *
   * The Save Replay Buffer row of the properties window edits OBS's own Save
   * Replay hotkey (FULL_BUFFER) instead of one of the plugin's.
   *
   * The OBS frontend reroutes hotkey callbacks to the Qt main thread, but runs
   * them while holding libobs's hotkey lock. Each callback posts the press and
   * returns, so the save (and any warning dialog it shows) runs after the lock
   * is released instead of stalling libobs's hotkey thread.
   */
  class HotkeyManager
  {
  public:
    /**
     * @brief Target index of OBS's own Save Replay hotkey in getAllBindings/setBinding
     */
    static constexpr size_t FULL_BUFFER = Config::MAX_SAVE_BUTTON_COUNT;

    //=========================================================================
    // CONSTRUCTORS & DESTRUCTOR
    //=========================================================================
    /**
     * @brief Constructor
     * @param context Object presses are posted to; delete this manager only
     *        while the context is being destroyed, so its queued presses go with it
     * @param saveSegmentCallback Callback for save segment hotkeys
     * @param buttons Current save buttons
     */
     HotkeyManager(
         QObject *context,
         std::function<void(int)> saveSegmentCallback,
         const std::vector<SaveButton> &buttons
    );

    /**
     * @brief Destructor
     *
     * Unregisters every hotkey. OBS holds its hotkey lock while it runs a
     * callback, so none is running once this returns.
     */
    ~HotkeyManager();

    //=========================================================================
    // HOTKEY MANAGEMENT
    //=========================================================================
    /**
     * @brief Registers all hotkeys with OBS
     *
     * Creates a hotkey for each save button. Users can assign key combinations to these hotkeys in OBS settings.
     */
    void registerHotkeys();

    /**
     * @brief Saves current hotkey bindings to disk
     *
     * Persists the bindings of every registered save button hotkey to the
     * plugin's config file, keeping the saved bindings of buttons that are not
     * registered. FULL_BUFFER is OBS's and is saved by setBinding(...).
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
     * @brief Gets the key combinations bound to every hotkey
     * @return One entry per save button slot, then FULL_BUFFER: every binding, in
     *         the order OBS holds them (empty if unbound). For a button that is not
     *         registered, the bindings saved in the config file. For FULL_BUFFER while
     *         OBS has no replay buffer output, the bindings in the profile config.
     *
     * Call only once OBS has finished loading (it reads the replay buffer output).
     */
    std::vector<std::vector<obs_key_combination_t>> getAllBindings() const;

    /**
     * @brief Replaces a hotkey's bindings with one key combination
     * @param target Save button index, or FULL_BUFFER
     * @param combination New binding, or nullptr to remove every binding
     *
     * FULL_BUFFER is written to the replay buffer output's hotkey and to the
     * profile config at once, as OBS Settings does, because OBS saves it only
     * when its own Settings are applied. Call only once OBS has finished loading.
     */
    void setBinding(size_t target, const obs_key_combination_t *combination);

  private:
    //=========================================================================
    // MEMBER VARIABLES
    //=========================================================================
    obs_hotkey_id saveHotkeys[Config::MAX_SAVE_BUTTON_COUNT]; ///< Hotkey ID of each save button slot
    QObject *context;                             ///< Receives presses once OBS's hotkey lock is released
    std::function<void(int)> onSaveSegment;       ///< Callback for save segment hotkeys
    std::vector<SaveButton> buttons;              ///< Current save buttons
    bool hotkeysRegistered = false;

    //=========================================================================
    // PRIVATE METHODS
    //=========================================================================
    obs_hotkey_id hotkeyId(size_t target) const;
    void registerButtonHotkey(size_t index);
    void setReplayBinding(const obs_key_combination_t *combination);
    void updateHotkeyDescriptions();

    /**
     * @brief Loads saved bindings from disk into the given hotkeys
     * @param targets Save button indexes
     */
    void loadHotkeySettings(const std::vector<size_t> &targets);
  };

} // namespace ReplayBufferPro

/**
 * @file hotkey-manager.cpp
 * @brief Implementation of hotkey management for the Replay Buffer Pro plugin
 */

#include "managers/hotkey-manager.hpp"
#include "utils/logger.hpp"
#include "utils/obs-utils.hpp"
#include "utils/duration-format.hpp"

// OBS includes
#include <util/platform.h>

// STL includes
#include <sstream>

namespace ReplayBufferPro
{
  namespace
  {
    constexpr const char *kHotkeyBindingsFile = "hotkey_bindings.json";
    constexpr const char *kFullBufferHotkeyName = "ReplayBufferPro.SaveFullBuffer";

    // Key of a hotkey's bindings in hotkey_bindings.json
    std::string bindingsKey(size_t target)
    {
      if (target == HotkeyManager::FULL_BUFFER)
        return "hotkey_full";
      return std::string("hotkey_") + std::to_string(target);
    }

    // Returns the saved bindings file, or nullptr if there is none
    obs_data_t *readBindingsFile()
    {
      char *config_path = obs_module_config_path(kHotkeyBindingsFile);
      if (!config_path)
      {
        Logger::error("Failed to get hotkey bindings config path");
        return nullptr;
      }

      obs_data_t *data = obs_data_create_from_json_file(config_path);
      bfree(config_path);
      return data;
    }

    // Same fields obs_hotkey_save(...) writes for each binding
    obs_key_combination_t decodeBinding(obs_data_t *item)
    {
      obs_key_combination_t combination = {0, OBS_KEY_NONE};
      if (obs_data_get_bool(item, "shift"))
        combination.modifiers |= INTERACT_SHIFT_KEY;
      if (obs_data_get_bool(item, "control"))
        combination.modifiers |= INTERACT_CONTROL_KEY;
      if (obs_data_get_bool(item, "alt"))
        combination.modifiers |= INTERACT_ALT_KEY;
      if (obs_data_get_bool(item, "command"))
        combination.modifiers |= INTERACT_COMMAND_KEY;
      combination.key = obs_key_from_name(obs_data_get_string(item, "key"));
      return combination;
    }
  } // namespace

  //=============================================================================
  // CONSTRUCTORS & DESTRUCTOR
  //=============================================================================

  HotkeyManager::HotkeyManager(
      std::function<void(int)> saveSegmentCallback,
      std::function<void()> saveFullBufferCallback,
      const std::vector<SaveButton> &buttons
  ) : onSaveSegment(saveSegmentCallback),
      onSaveFullBuffer(saveFullBufferCallback),
      buttons(buttons)
  {
    // Initialize hotkey IDs to invalid
    for (size_t i = 0; i < Config::MAX_SAVE_BUTTON_COUNT; i++) {
      saveHotkeys[i] = OBS_INVALID_HOTKEY_ID;
    }
  }

  //=============================================================================
  // HOTKEY MANAGEMENT
  //=============================================================================

  void HotkeyManager::registerHotkeys()
  {
    std::vector<size_t> targets;
    for (size_t i = 0; i < buttons.size(); i++) {
      registerButtonHotkey(i);
      targets.push_back(i);
    }

    saveFullHotkey = obs_hotkey_register_frontend(
      kFullBufferHotkeyName,
      obs_module_text("SaveFullHotkey"),
      [](void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed) {
        auto self = static_cast<HotkeyManager *>(data);
        if (pressed && self->onSaveFullBuffer) {
          self->onSaveFullBuffer();
        }
      },
      this
    );
    targets.push_back(FULL_BUFFER);

    // Load saved hotkey bindings after registration
    loadHotkeySettings(targets);
    hotkeysRegistered = true;
  }

  void HotkeyManager::registerButtonHotkey(size_t index)
  {
    std::string name = std::string("ReplayBufferPro.SaveButton") + std::to_string(index + 1);
    std::string description =
      formatButtonHotkeyDescription(buttons[index].seconds, buttons[index].label).toUtf8().constData();

    // Capture 'this' and button index for the callback
    saveHotkeys[index] = obs_hotkey_register_frontend(
      name.c_str(),
      description.c_str(),
      [](void *data, obs_hotkey_id id, obs_hotkey_t *, bool pressed) {
        if (pressed) {
          auto self = static_cast<HotkeyManager *>(data);
          int duration = 0;

          // Find which hotkey was pressed by matching the ID
          for (size_t i = 0; i < self->buttons.size(); i++) {
            if (self->saveHotkeys[i] == id) {
              duration = self->buttons[i].seconds;
              break;
            }
          }

          if (duration > 0 && self->onSaveSegment) {
            self->onSaveSegment(duration);
          }
        }
      },
      this
    );

    Logger::info("Registered hotkey for save button %zu", index + 1);
  }

  void HotkeyManager::setButtons(const std::vector<SaveButton> &newButtons)
  {
    if (!hotkeysRegistered)
    {
      buttons = newButtons;
      return;
    }

    size_t oldCount = buttons.size();
    size_t newCount = newButtons.size();

    // Keep the bindings of buttons about to go, so raising the count restores them
    if (newCount < oldCount)
    {
      saveHotkeySettings();
      for (size_t i = newCount; i < oldCount; i++)
      {
        obs_hotkey_unregister(saveHotkeys[i]);
        saveHotkeys[i] = OBS_INVALID_HOTKEY_ID;
        Logger::info("Unregistered hotkey for save button %zu", i + 1);
      }
    }

    buttons = newButtons;

    if (newCount > oldCount)
    {
      std::vector<size_t> added;
      for (size_t i = oldCount; i < newCount; i++)
      {
        registerButtonHotkey(i);
        added.push_back(i);
      }
      loadHotkeySettings(added);
    }

    updateHotkeyDescriptions();
  }

  //=============================================================================
  // HOTKEY BINDINGS
  //=============================================================================

  obs_hotkey_id HotkeyManager::hotkeyId(size_t target) const
  {
    if (target == FULL_BUFFER)
      return saveFullHotkey;
    if (target < Config::MAX_SAVE_BUTTON_COUNT)
      return saveHotkeys[target];
    return OBS_INVALID_HOTKEY_ID;
  }

  std::vector<obs_key_combination_t> HotkeyManager::getBindings(size_t target) const
  {
    struct Query
    {
      obs_hotkey_id id;
      std::vector<obs_key_combination_t> bindings;
    } query{hotkeyId(target), {}};

    if (query.id == OBS_INVALID_HOTKEY_ID)
    {
      // Not registered: the bindings it had when the button count went down
      OBSDataRAII data(readBindingsFile());
      if (!data.isValid())
        return query.bindings;

      obs_data_array_t *array = obs_data_get_array(data.get(), bindingsKey(target).c_str());
      if (!array)
        return query.bindings;

      for (size_t i = 0; i < obs_data_array_count(array); i++)
      {
        obs_data_t *item = obs_data_array_item(array, i);
        obs_key_combination_t combination = decodeBinding(item);
        if (!obs_key_combination_is_empty(combination))
          query.bindings.push_back(combination);
        obs_data_release(item);
      }
      obs_data_array_release(array);
      return query.bindings;
    }

    obs_enum_hotkey_bindings(
      [](void *data, size_t, obs_hotkey_binding_t *binding) {
        auto q = static_cast<Query *>(data);
        if (obs_hotkey_binding_get_hotkey_id(binding) == q->id)
          q->bindings.push_back(obs_hotkey_binding_get_key_combination(binding));
        return true;
      },
      &query);
    return query.bindings;
  }

  void HotkeyManager::setBinding(size_t target, const obs_key_combination_t *combination)
  {
    obs_hotkey_id id = hotkeyId(target);
    if (id == OBS_INVALID_HOTKEY_ID)
      return;

    obs_key_combination_t combo = combination ? *combination : obs_key_combination_t{0, OBS_KEY_NONE};
    obs_hotkey_load_bindings(id, &combo, combination ? 1 : 0);
  }

  //=============================================================================
  // HOTKEY PERSISTENCE
  //=============================================================================

  void HotkeyManager::saveHotkeySettings()
  {
    // Start from the saved file so buttons that are not registered keep their bindings
    OBSDataRAII data(readBindingsFile());
    OBSDataRAII fresh(data.isValid() ? nullptr : obs_data_create());
    obs_data_t *target = data.isValid() ? data.get() : fresh.get();
    if (!target)
      return;

    std::vector<size_t> targets;
    for (size_t i = 0; i < Config::MAX_SAVE_BUTTON_COUNT; i++)
      targets.push_back(i);
    targets.push_back(FULL_BUFFER);

    for (size_t t : targets) {
      obs_hotkey_id id = hotkeyId(t);
      if (id == OBS_INVALID_HOTKEY_ID)
        continue;

      obs_data_array_t *hotkeyArray = obs_hotkey_save(id);
      if (hotkeyArray) {
        obs_data_set_array(target, bindingsKey(t).c_str(), hotkeyArray);
        obs_data_array_release(hotkeyArray);
      }
    }

    // Save to config file
    char *config_dir = obs_module_config_path("");
    if (!config_dir)
    {
      Logger::error("Failed to get config directory path");
      return;
    }

    if (os_mkdirs(config_dir) < 0)
    {
      Logger::error("Failed to create config directory: %s", config_dir);
      bfree(config_dir);
      return;
    }

    std::string config_path = std::string(config_dir) + "/" + kHotkeyBindingsFile;
    bfree(config_dir);

    if (!obs_data_save_json_safe(target, config_path.c_str(),
                                 Config::TEMP_FILE_SUFFIX, Config::BACKUP_FILE_SUFFIX))
    {
      Logger::error("Failed to save hotkey bindings to: %s", config_path.c_str());
    }
    else
    {
      Logger::info("Saved hotkey bindings to: %s", config_path.c_str());
    }
  }

  void HotkeyManager::loadHotkeySettings(const std::vector<size_t> &targets)
  {
    OBSDataRAII data(readBindingsFile());

    if (!data.isValid())
    {
      Logger::info("No saved hotkey bindings found");
      return;
    }

    for (size_t t : targets) {
      obs_hotkey_id id = hotkeyId(t);
      if (id == OBS_INVALID_HOTKEY_ID)
        continue;

      obs_data_array_t *hotkeyArray = obs_data_get_array(data.get(), bindingsKey(t).c_str());
      if (hotkeyArray) {
        obs_hotkey_load(id, hotkeyArray);
        obs_data_array_release(hotkeyArray);
      }
    }

    Logger::info("Loaded hotkey bindings");
  }

  void HotkeyManager::updateHotkeyDescriptions()
  {
    for (size_t i = 0; i < buttons.size(); i++)
    {
      if (saveHotkeys[i] == OBS_INVALID_HOTKEY_ID)
      {
        continue;
      }

      std::string description =
        formatButtonHotkeyDescription(buttons[i].seconds, buttons[i].label).toUtf8().constData();
      obs_hotkey_set_description(saveHotkeys[i], description.c_str());
    }
  }

} // namespace ReplayBufferPro

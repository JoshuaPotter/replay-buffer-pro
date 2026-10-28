/**
 * @file hotkey-manager.cpp
 * @brief Implementation of hotkey management for the Replay Buffer Pro plugin
 */

#include "managers/hotkey-manager.hpp"
#include "utils/logger.hpp"
#include "utils/obs-utils.hpp"
#include "utils/duration-format.hpp"

// OBS includes
#include <obs.hpp>
#include <util/config-file.h>
#include <util/platform.h>

// STL includes
#include <cstring>
#include <sstream>

namespace ReplayBufferPro
{
  namespace
  {
    constexpr const char *kHotkeyBindingsFile = "hotkey_bindings.json";

    // OBS's own Save Replay hotkey (obs-ffmpeg-mux.c). OBS registers it on the
    // replay buffer output, which exists only while the replay buffer is on in
    // its Output settings, and keeps its bindings in the profile config.
    constexpr const char *kReplayHotkeyName = "ReplayBuffer.Save";
    constexpr const char *kReplayHotkeySection = "Hotkeys";
    constexpr const char *kReplayHotkeyKey = "ReplayBuffer";

    // Key of a save button hotkey's bindings in hotkey_bindings.json
    std::string bindingsKey(size_t target)
    {
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

    // Inverse of decodeBinding(...)
    obs_data_t *encodeBinding(obs_key_combination_t combination)
    {
      obs_data_t *item = obs_data_create();
      obs_data_set_bool(item, "shift", (combination.modifiers & INTERACT_SHIFT_KEY) != 0);
      obs_data_set_bool(item, "control", (combination.modifiers & INTERACT_CONTROL_KEY) != 0);
      obs_data_set_bool(item, "alt", (combination.modifiers & INTERACT_ALT_KEY) != 0);
      obs_data_set_bool(item, "command", (combination.modifiers & INTERACT_COMMAND_KEY) != 0);
      obs_data_set_string(item, "key", obs_key_to_name(combination.key));
      return item;
    }

    // Bindings of the replay buffer output's hotkeys, keyed by hotkey name: the
    // live ones while the output exists, else what the profile config holds for
    // the next output OBS creates. nullptr if there are none.
    obs_data_t *readReplayHotkeys(obs_output_t *output)
    {
      if (output)
        return obs_hotkeys_save_output(output);

      const char *json = config_get_string(obs_frontend_get_profile_config(),
                                           kReplayHotkeySection, kReplayHotkeyKey);
      return json ? obs_data_create_from_json(json) : nullptr;
    }

    // ID of the Save Replay hotkey OBS registered on the output
    obs_hotkey_id findReplayHotkey(obs_output_t *output)
    {
      struct Query
      {
        obs_weak_output_t *weak;
        obs_hotkey_id id;
      } query{obs_output_get_weak_output(output), OBS_INVALID_HOTKEY_ID};

      obs_enum_hotkeys(
        [](void *data, obs_hotkey_id id, obs_hotkey_t *key) {
          auto q = static_cast<Query *>(data);
          if (obs_hotkey_get_registerer_type(key) == OBS_HOTKEY_REGISTERER_OUTPUT &&
              obs_hotkey_get_registerer(key) == q->weak &&
              strcmp(obs_hotkey_get_name(key), kReplayHotkeyName) == 0)
          {
            q->id = id;
            return false;
          }
          return true;
        },
        &query);

      obs_weak_output_release(query.weak);
      return query.id;
    }
  } // namespace

  //=============================================================================
  // CONSTRUCTORS & DESTRUCTOR
  //=============================================================================

  HotkeyManager::HotkeyManager(
      QObject *context,
      std::function<void(int)> saveSegmentCallback,
      const std::vector<SaveButton> &buttons
  ) : context(context),
      onSaveSegment(saveSegmentCallback),
      buttons(buttons)
  {
    // Initialize hotkey IDs to invalid
    for (size_t i = 0; i < Config::MAX_SAVE_BUTTON_COUNT; i++) {
      saveHotkeys[i] = OBS_INVALID_HOTKEY_ID;
    }
  }

  HotkeyManager::~HotkeyManager()
  {
    for (size_t i = 0; i < Config::MAX_SAVE_BUTTON_COUNT; i++) {
      if (saveHotkeys[i] != OBS_INVALID_HOTKEY_ID) {
        obs_hotkey_unregister(saveHotkeys[i]);
      }
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
        if (!pressed)
          return;
        // Runs under libobs's hotkey lock: post and return, so the save runs
        // after it is released. The manager is deleted only while its context
        // is destroyed, which drops the context's queued events, so a queued
        // press never reaches a deleted manager.
        auto self = static_cast<HotkeyManager *>(data);
        QMetaObject::invokeMethod(self->context, [self, id]() {
          int duration = 0;

          // Find which hotkey was pressed by matching the ID; a button removed
          // since the press no longer matches
          for (size_t i = 0; i < self->buttons.size(); i++) {
            if (self->saveHotkeys[i] == id) {
              duration = self->buttons[i].seconds;
              break;
            }
          }

          if (duration > 0 && self->onSaveSegment) {
            self->onSaveSegment(duration);
          }
        }, Qt::QueuedConnection);
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
    if (target < Config::MAX_SAVE_BUTTON_COUNT)
      return saveHotkeys[target];
    return OBS_INVALID_HOTKEY_ID;
  }

  std::vector<std::vector<obs_key_combination_t>> HotkeyManager::getAllBindings() const
  {
    std::vector<std::vector<obs_key_combination_t>> all(FULL_BUFFER + 1);

    // Read once, for every button that is not registered: the bindings it had
    // when the button count went down
    OBSDataRAII saved(readBindingsFile());

    for (size_t target = 0; target < FULL_BUFFER; target++)
    {
      struct Query
      {
        obs_hotkey_id id;
        std::vector<obs_key_combination_t> &bindings;
      } query{hotkeyId(target), all[target]};

      if (query.id == OBS_INVALID_HOTKEY_ID)
      {
        if (!saved.isValid())
          continue;

        obs_data_array_t *array = obs_data_get_array(saved.get(), bindingsKey(target).c_str());
        if (!array)
          continue;

        for (size_t i = 0; i < obs_data_array_count(array); i++)
        {
          obs_data_t *item = obs_data_array_item(array, i);
          obs_key_combination_t combination = decodeBinding(item);
          if (!obs_key_combination_is_empty(combination))
            query.bindings.push_back(combination);
          obs_data_release(item);
        }
        obs_data_array_release(array);
        continue;
      }

      obs_enum_hotkey_bindings(
        [](void *data, size_t, obs_hotkey_binding_t *binding) {
          auto q = static_cast<Query *>(data);
          if (obs_hotkey_binding_get_hotkey_id(binding) == q->id)
            q->bindings.push_back(obs_hotkey_binding_get_key_combination(binding));
          return true;
        },
        &query);
    }

    OBSOutputAutoRelease output(obs_frontend_get_replay_buffer_output());
    OBSDataRAII replayHotkeys(readReplayHotkeys(output));
    if (replayHotkeys.isValid())
    {
      obs_data_array_t *array = obs_data_get_array(replayHotkeys.get(), kReplayHotkeyName);
      for (size_t i = 0; array && i < obs_data_array_count(array); i++)
      {
        obs_data_t *item = obs_data_array_item(array, i);
        obs_key_combination_t combination = decodeBinding(item);
        if (!obs_key_combination_is_empty(combination))
          all[FULL_BUFFER].push_back(combination);
        obs_data_release(item);
      }
      obs_data_array_release(array);
    }

    return all;
  }

  void HotkeyManager::setBinding(size_t target, const obs_key_combination_t *combination)
  {
    if (target == FULL_BUFFER)
    {
      setReplayBinding(combination);
      return;
    }

    obs_hotkey_id id = hotkeyId(target);
    if (id == OBS_INVALID_HOTKEY_ID)
      return;

    obs_key_combination_t combo = combination ? *combination : obs_key_combination_t{0, OBS_KEY_NONE};
    obs_hotkey_load_bindings(id, &combo, combination ? 1 : 0);
  }

  void HotkeyManager::setReplayBinding(const obs_key_combination_t *combination)
  {
    OBSOutputAutoRelease output(obs_frontend_get_replay_buffer_output());
    obs_hotkey_id id = output ? findReplayHotkey(output) : OBS_INVALID_HOTKEY_ID;
    if (id != OBS_INVALID_HOTKEY_ID)
    {
      obs_key_combination_t combo = combination ? *combination : obs_key_combination_t{0, OBS_KEY_NONE};
      obs_hotkey_load_bindings(id, &combo, combination ? 1 : 0);
    }

    // Read back from the output, or edit what the profile config holds for the
    // next output OBS creates
    obs_data_t *read = readReplayHotkeys(id != OBS_INVALID_HOTKEY_ID ? output.Get() : nullptr);
    OBSDataRAII hotkeys(read ? read : obs_data_create());
    if (id == OBS_INVALID_HOTKEY_ID)
    {
      obs_data_array_t *array = obs_data_array_create();
      if (combination)
      {
        obs_data_t *item = encodeBinding(*combination);
        obs_data_array_push_back(array, item);
        obs_data_release(item);
      }
      obs_data_set_array(hotkeys.get(), kReplayHotkeyName, array);
      obs_data_array_release(array);
    }

    // Where OBS Settings saves it (SaveHotkeySettings) and the output reads it
    config_t *config = obs_frontend_get_profile_config();
    config_set_string(config, kReplayHotkeySection, kReplayHotkeyKey, obs_data_get_json(hotkeys.get()));
    if (config_save_safe(config, "tmp", nullptr) != CONFIG_SUCCESS)
    {
      Logger::error("Failed to save OBS's Save Replay hotkey to the profile config");
    }
  }

  //=============================================================================
  // HOTKEY PERSISTENCE
  //=============================================================================

  void HotkeyManager::saveHotkeySettings()
  {
    // Start from the saved file so buttons that are not registered keep their bindings
    obs_data_t *saved = readBindingsFile();
    OBSDataRAII data(saved ? saved : obs_data_create());
    if (!data.isValid())
      return;

    // Every save button slot
    for (size_t t = 0; t < FULL_BUFFER; t++) {
      obs_hotkey_id id = hotkeyId(t);
      if (id == OBS_INVALID_HOTKEY_ID)
        continue;

      obs_data_array_t *hotkeyArray = obs_hotkey_save(id);
      if (hotkeyArray) {
        obs_data_set_array(data.get(), bindingsKey(t).c_str(), hotkeyArray);
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

    if (!obs_data_save_json_safe(data.get(), config_path.c_str(),
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

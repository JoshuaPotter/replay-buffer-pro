/**
 * @file hotkey-manager.cpp
 * @brief Implementation of hotkey management for the Replay Buffer Pro plugin
 */

#include "managers/hotkey-manager.hpp"
#include "utils/logger.hpp"
#include "utils/obs-utils.hpp"

// OBS includes
#include <util/platform.h>

// STL includes
#include <sstream>

namespace ReplayBufferPro
{
  //=============================================================================
  // CONSTRUCTORS & DESTRUCTOR
  //=============================================================================

  HotkeyManager::HotkeyManager(
      std::function<void(int)> saveSegmentCallback
  ) : onSaveSegment(saveSegmentCallback)
  {
    // Initialize hotkey IDs to invalid
    for (size_t i = 0; i < Config::SAVE_BUTTON_COUNT; i++) {
      saveHotkeys[i] = OBS_INVALID_HOTKEY_ID;
    }
  }

  //=============================================================================
  // HOTKEY MANAGEMENT
  //=============================================================================

  void HotkeyManager::registerHotkeys()
  {
    // Register hotkeys for each save duration
    for (size_t i = 0; i < Config::SAVE_BUTTON_COUNT; i++) {
      const auto &btn = Config::SAVE_BUTTONS[i];
      std::string name = std::string("ReplayBufferPro.Save") + std::to_string(btn.duration) + "Sec";
      std::string description = std::string("Save Last ") + obs_module_text(btn.text);
      
      // Capture 'this' and button index for the callback
      saveHotkeys[i] = obs_hotkey_register_frontend(
        name.c_str(),
        description.c_str(),
        [](void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed) {
          if (pressed) {
            auto self = static_cast<HotkeyManager *>(data);
            int duration = 0;
            
            // Find which hotkey was pressed by matching the ID
            for (size_t i = 0; i < Config::SAVE_BUTTON_COUNT; i++) {
              if (self->saveHotkeys[i] == id) {
                duration = Config::SAVE_BUTTONS[i].duration;
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
      
      Logger::info("Registered hotkey for saving %d seconds", btn.duration);
    }

    // Load saved hotkey bindings after registration
    loadHotkeySettings();
  }

  //=============================================================================
  // HOTKEY PERSISTENCE
  //=============================================================================

  void HotkeyManager::saveHotkeySettings()
  {
    OBSDataRAII data(obs_data_create());
    if (!data.isValid())
      return;

    // Save each hotkey's bindings
    for (size_t i = 0; i < Config::SAVE_BUTTON_COUNT; i++) {
      if (saveHotkeys[i] != OBS_INVALID_HOTKEY_ID) {
        std::string key = std::string("hotkey_") + std::to_string(i);
        obs_data_array_t *hotkeyArray = obs_hotkey_save(saveHotkeys[i]);
        if (hotkeyArray) {
          obs_data_set_array(data.get(), key.c_str(), hotkeyArray);
          obs_data_array_release(hotkeyArray);
        }
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

    std::string config_path = std::string(config_dir) + "/hotkey_bindings.json";
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

  void HotkeyManager::loadHotkeySettings()
  {
    char *config_path = obs_module_config_path("hotkey_bindings.json");
    if (!config_path)
    {
      Logger::error("Failed to get hotkey bindings config path");
      return;
    }

    OBSDataRAII data(obs_data_create_from_json_file(config_path));
    bfree(config_path);

    if (!data.isValid())
    {
      Logger::info("No saved hotkey bindings found");
      return;
    }

    // Load each hotkey's bindings
    for (size_t i = 0; i < Config::SAVE_BUTTON_COUNT; i++) {
      if (saveHotkeys[i] != OBS_INVALID_HOTKEY_ID) {
        std::string key = std::string("hotkey_") + std::to_string(i);
        obs_data_array_t *hotkeyArray = obs_data_get_array(data.get(), key.c_str());
        if (hotkeyArray) {
          obs_hotkey_load(saveHotkeys[i], hotkeyArray);
          obs_data_array_release(hotkeyArray);
        }
      }
    }

    Logger::info("Loaded hotkey bindings");
  }

} // namespace ReplayBufferPro 
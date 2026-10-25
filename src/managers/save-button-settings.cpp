/**
 * @file save-button-settings.cpp
 * @brief Global settings for save buttons and other plugin properties
 */

#include "managers/save-button-settings.hpp"
#include "config/config.hpp"
#include "utils/logger.hpp"
#include "utils/obs-utils.hpp"

// OBS includes
#include <obs-module.h>
#include <util/platform.h>

// STL includes
#include <algorithm>

namespace ReplayBufferPro
{
  namespace
  {
    constexpr const char *kSaveButtonSettingsFile = "save_button_settings.json";
    constexpr const char *kSaveButtonSettingsKey = "save_buttons";
    constexpr const char *kSaveButtonSettingsSecondsKey = "seconds";
    constexpr const char *kSaveButtonSettingsLabelKey = "label";
    constexpr const char *kSaveButtonSettingsVersionKey = "version";
    constexpr const char *kInMemoryClipsKey = "in_memory_clips";
    // 2 added labels and a variable button count; version 1 files (six
    // buttons, no labels) load unchanged
    constexpr int kSaveButtonSettingsVersion = 2;
  } // namespace

  SaveButtonSettings::SaveButtonSettings()
      : buttons(getDefaultButtons())
  {
  }

  const std::vector<SaveButton> &SaveButtonSettings::getButtons() const
  {
    return buttons;
  }

  void SaveButtonSettings::setButtons(const std::vector<SaveButton> &values)
  {
    buttons = normalizeButtons(values);
  }

  bool SaveButtonSettings::getInMemoryClips() const
  {
    return inMemoryClips;
  }

  void SaveButtonSettings::setInMemoryClips(bool enabled)
  {
    inMemoryClips = enabled;
  }

  void SaveButtonSettings::load()
  {
    buttons = getDefaultButtons();
    inMemoryClips = false;

    std::string configPath = getConfigPath();
    if (configPath.empty())
    {
      Logger::error("Failed to resolve save button settings path");
      return;
    }

    OBSDataRAII data(obs_data_create_from_json_file(configPath.c_str()));
    if (!data.isValid())
    {
      Logger::info("No save button settings found; using defaults");
      return;
    }

    inMemoryClips = obs_data_get_bool(data.get(), kInMemoryClipsKey);

    obs_data_array_t *array = obs_data_get_array(data.get(), kSaveButtonSettingsKey);
    if (!array)
    {
      Logger::warning("Save button settings file missing array; using defaults");
      return;
    }

    std::vector<SaveButton> loadedValues;
    size_t count = obs_data_array_count(array);
    loadedValues.reserve(count);

    for (size_t i = 0; i < count; i++)
    {
      obs_data_t *item = obs_data_array_item(array, i);
      if (item)
      {
        int seconds = static_cast<int>(obs_data_get_int(item, kSaveButtonSettingsSecondsKey));
        loadedValues.push_back({seconds, obs_data_get_string(item, kSaveButtonSettingsLabelKey)});
        obs_data_release(item);
      }
    }

    obs_data_array_release(array);
    buttons = normalizeButtons(loadedValues);
  }

  bool SaveButtonSettings::save() const
  {
    OBSDataRAII data(obs_data_create());
    if (!data.isValid())
    {
      Logger::error("Failed to create save button settings data");
      return false;
    }

    obs_data_set_int(data.get(), kSaveButtonSettingsVersionKey, kSaveButtonSettingsVersion);
    obs_data_set_bool(data.get(), kInMemoryClipsKey, inMemoryClips);

    obs_data_array_t *array = obs_data_array_create();
    for (const auto &button : buttons)
    {
      obs_data_t *item = obs_data_create();
      obs_data_set_int(item, kSaveButtonSettingsSecondsKey, button.seconds);
      obs_data_set_string(item, kSaveButtonSettingsLabelKey, button.label.c_str());
      obs_data_array_push_back(array, item);
      obs_data_release(item);
    }

    obs_data_set_array(data.get(), kSaveButtonSettingsKey, array);
    obs_data_array_release(array);

    std::string configPath = getConfigPath();
    if (configPath.empty())
    {
      Logger::error("Failed to resolve save button settings path");
      return false;
    }

    std::string configDir = configPath.substr(0, configPath.find_last_of("/\\"));
    if (!configDir.empty() && os_mkdirs(configDir.c_str()) < 0)
    {
      Logger::error("Failed to create save button settings directory: %s", configDir.c_str());
      return false;
    }

    if (!obs_data_save_json_safe(data.get(), configPath.c_str(),
                                 Config::TEMP_FILE_SUFFIX, Config::BACKUP_FILE_SUFFIX))
    {
      Logger::error("Failed to save save button settings to: %s", configPath.c_str());
      return false;
    }

    Logger::info("Saved save button settings to: %s", configPath.c_str());
    return true;
  }

  std::vector<SaveButton> SaveButtonSettings::getDefaultButtons()
  {
    std::vector<SaveButton> defaults;
    defaults.reserve(Config::DEFAULT_SAVE_BUTTON_COUNT);
    for (size_t i = 0; i < Config::DEFAULT_SAVE_BUTTON_COUNT; i++)
    {
      defaults.push_back({Config::SAVE_BUTTON_DEFAULTS[i], std::string()});
    }
    return defaults;
  }

  std::vector<SaveButton> SaveButtonSettings::normalizeButtons(const std::vector<SaveButton> &input) const
  {
    if (input.empty())
    {
      return getDefaultButtons();
    }

    std::vector<SaveButton> normalized(input.begin(),
                                       input.begin() + std::min(input.size(), Config::MAX_SAVE_BUTTON_COUNT));
    for (auto &button : normalized)
    {
      button.seconds = std::max(1, std::min(button.seconds, Config::MAX_BUFFER_LENGTH));
    }
    return normalized;
  }

  std::string SaveButtonSettings::getConfigPath() const
  {
    char *configPath = obs_module_config_path(kSaveButtonSettingsFile);
    if (!configPath)
    {
      return std::string();
    }

    std::string path(configPath);
    bfree(configPath);
    return path;
  }
} // namespace ReplayBufferPro

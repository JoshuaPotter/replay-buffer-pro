/**
 * @file save-button-settings.cpp
 * @brief Global settings for save button durations
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
    constexpr const char *kSaveButtonSettingsVersionKey = "version";
    constexpr int kSaveButtonSettingsVersion = 1;
  } // namespace

  SaveButtonSettings::SaveButtonSettings()
      : durations(getDefaultDurations())
  {
  }

  const std::vector<int> &SaveButtonSettings::getDurations() const
  {
    return durations;
  }

  void SaveButtonSettings::setDurations(const std::vector<int> &values)
  {
    durations = normalizeDurations(values);
  }

  void SaveButtonSettings::load()
  {
    durations = getDefaultDurations();

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

    obs_data_array_t *array = obs_data_get_array(data.get(), kSaveButtonSettingsKey);
    if (!array)
    {
      Logger::warning("Save button settings file missing array; using defaults");
      return;
    }

    std::vector<int> loadedValues;
    size_t count = obs_data_array_count(array);
    loadedValues.reserve(count);

    for (size_t i = 0; i < count; i++)
    {
      obs_data_t *item = obs_data_array_item(array, i);
      if (item)
      {
        int seconds = static_cast<int>(obs_data_get_int(item, kSaveButtonSettingsSecondsKey));
        loadedValues.push_back(seconds);
        obs_data_release(item);
      }
    }

    obs_data_array_release(array);
    durations = normalizeDurations(loadedValues);
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

    obs_data_array_t *array = obs_data_array_create();
    for (size_t i = 0; i < durations.size(); i++)
    {
      obs_data_t *item = obs_data_create();
      obs_data_set_int(item, kSaveButtonSettingsSecondsKey, durations[i]);
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

  std::vector<int> SaveButtonSettings::getDefaultDurations()
  {
    std::vector<int> defaults;
    defaults.reserve(Config::SAVE_BUTTON_COUNT);
    for (size_t i = 0; i < Config::SAVE_BUTTON_COUNT; i++)
    {
      defaults.push_back(Config::SAVE_BUTTONS[i].duration);
    }
    return defaults;
  }

  std::vector<int> SaveButtonSettings::normalizeDurations(const std::vector<int> &input) const
  {
    std::vector<int> normalized = getDefaultDurations();
    size_t limit = std::min(input.size(), normalized.size());
    for (size_t i = 0; i < limit; i++)
    {
      int value = input[i];
      value = std::max(1, std::min(value, Config::MAX_BUFFER_LENGTH));
      normalized[i] = value;
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

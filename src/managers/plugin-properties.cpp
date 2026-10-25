/**
 * @file plugin-properties.cpp
 * @brief Global plugin properties: save buttons, experimental options, window size
 */

#include "managers/plugin-properties.hpp"
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
    constexpr const char *kPropertiesFile = "properties.json";
    // Earlier name of the file; read when properties.json does not exist yet and
    // left in place, so going back to an older version still finds its settings
    constexpr const char *kLegacyPropertiesFile = "save_button_settings.json";
    constexpr const char *kSaveButtonsKey = "save_buttons";
    constexpr const char *kSecondsKey = "seconds";
    constexpr const char *kLabelKey = "label";
    constexpr const char *kVersionKey = "version";
    constexpr const char *kInMemoryClipsKey = "in_memory_clips";
    constexpr const char *kWindowWidthKey = "properties_window_width";
    constexpr const char *kWindowHeightKey = "properties_window_height";
    // 2 added labels and a variable button count; version 1 files (six
    // buttons, no labels) load unchanged
    constexpr int kPropertiesVersion = 2;

    std::string moduleConfigPath(const char *file)
    {
      char *configPath = obs_module_config_path(file);
      if (!configPath)
      {
        return std::string();
      }

      std::string path(configPath);
      bfree(configPath);
      return path;
    }
  } // namespace

  PluginProperties::PluginProperties()
      : buttons(getDefaultButtons())
  {
  }

  const std::vector<SaveButton> &PluginProperties::getButtons() const
  {
    return buttons;
  }

  void PluginProperties::setButtons(const std::vector<SaveButton> &values)
  {
    buttons = normalizeButtons(values);
  }

  bool PluginProperties::getInMemoryClips() const
  {
    return inMemoryClips;
  }

  void PluginProperties::setInMemoryClips(bool enabled)
  {
    inMemoryClips = enabled;
  }

  int PluginProperties::getWindowWidth() const
  {
    return windowWidth;
  }

  int PluginProperties::getWindowHeight() const
  {
    return windowHeight;
  }

  void PluginProperties::setWindowSize(int width, int height)
  {
    windowWidth = std::max(0, width);
    windowHeight = std::max(0, height);
  }

  void PluginProperties::load()
  {
    buttons = getDefaultButtons();
    inMemoryClips = false;
    windowWidth = 0;
    windowHeight = 0;

    std::string configPath = moduleConfigPath(kPropertiesFile);
    if (configPath.empty())
    {
      Logger::error("Failed to resolve properties path");
      return;
    }

    OBSDataRAII data(obs_data_create_from_json_file(configPath.c_str()));
    if (!data.isValid())
    {
      std::string legacyPath = moduleConfigPath(kLegacyPropertiesFile);
      OBSDataRAII legacy(legacyPath.empty() ? nullptr : obs_data_create_from_json_file(legacyPath.c_str()));
      if (!legacy.isValid())
      {
        Logger::info("No properties found; using defaults");
        return;
      }

      Logger::info("Loading properties from %s", kLegacyPropertiesFile);
      loadFrom(legacy.get());
      return;
    }

    loadFrom(data.get());
  }

  void PluginProperties::loadFrom(obs_data_t *data)
  {
    inMemoryClips = obs_data_get_bool(data, kInMemoryClipsKey);
    setWindowSize(static_cast<int>(obs_data_get_int(data, kWindowWidthKey)),
                  static_cast<int>(obs_data_get_int(data, kWindowHeightKey)));

    obs_data_array_t *array = obs_data_get_array(data, kSaveButtonsKey);
    if (!array)
    {
      Logger::warning("Properties file missing save buttons; using defaults");
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
        int seconds = static_cast<int>(obs_data_get_int(item, kSecondsKey));
        loadedValues.push_back({seconds, obs_data_get_string(item, kLabelKey)});
        obs_data_release(item);
      }
    }

    obs_data_array_release(array);
    buttons = normalizeButtons(loadedValues);
  }

  bool PluginProperties::save() const
  {
    OBSDataRAII data(obs_data_create());
    if (!data.isValid())
    {
      Logger::error("Failed to create properties data");
      return false;
    }

    obs_data_set_int(data.get(), kVersionKey, kPropertiesVersion);
    obs_data_set_bool(data.get(), kInMemoryClipsKey, inMemoryClips);
    if (windowWidth > 0 && windowHeight > 0)
    {
      obs_data_set_int(data.get(), kWindowWidthKey, windowWidth);
      obs_data_set_int(data.get(), kWindowHeightKey, windowHeight);
    }

    obs_data_array_t *array = obs_data_array_create();
    for (const auto &button : buttons)
    {
      obs_data_t *item = obs_data_create();
      obs_data_set_int(item, kSecondsKey, button.seconds);
      obs_data_set_string(item, kLabelKey, button.label.c_str());
      obs_data_array_push_back(array, item);
      obs_data_release(item);
    }

    obs_data_set_array(data.get(), kSaveButtonsKey, array);
    obs_data_array_release(array);

    std::string configPath = moduleConfigPath(kPropertiesFile);
    if (configPath.empty())
    {
      Logger::error("Failed to resolve properties path");
      return false;
    }

    std::string configDir = configPath.substr(0, configPath.find_last_of("/\\"));
    if (!configDir.empty() && os_mkdirs(configDir.c_str()) < 0)
    {
      Logger::error("Failed to create properties directory: %s", configDir.c_str());
      return false;
    }

    if (!obs_data_save_json_safe(data.get(), configPath.c_str(),
                                 Config::TEMP_FILE_SUFFIX, Config::BACKUP_FILE_SUFFIX))
    {
      Logger::error("Failed to save properties to: %s", configPath.c_str());
      return false;
    }

    Logger::info("Saved properties to: %s", configPath.c_str());
    return true;
  }

  std::vector<SaveButton> PluginProperties::getDefaultButtons()
  {
    std::vector<SaveButton> defaults;
    defaults.reserve(Config::DEFAULT_SAVE_BUTTON_COUNT);
    for (size_t i = 0; i < Config::DEFAULT_SAVE_BUTTON_COUNT; i++)
    {
      defaults.push_back({Config::SAVE_BUTTON_DEFAULTS[i], std::string()});
    }
    return defaults;
  }

  std::vector<SaveButton> PluginProperties::normalizeButtons(const std::vector<SaveButton> &input) const
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
} // namespace ReplayBufferPro

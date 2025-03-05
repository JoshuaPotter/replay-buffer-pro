/**
 * @file settings-manager.cpp
 * @brief Implementation of settings management for the Replay Buffer Pro plugin
 */

#include "managers/settings-manager.hpp"
#include "config/config.hpp"
#include "utils/logger.hpp"
#include "utils/obs-utils.hpp"

// Qt includes
#include <QMessageBox>

namespace ReplayBufferPro
{
  //=============================================================================
  // SETTINGS MANAGEMENT
  //=============================================================================

  ConfigContext SettingsManager::getConfigContext()
  {
    config_t *config = obs_frontend_get_profile_config();
    if (!config)
    {
      throw std::runtime_error("Failed to get OBS profile config");
    }

    const char *mode = config_get_string(config, "Output", "Mode");
    const char *section = (mode && strcmp(mode, "Advanced") == 0) ? "AdvOut" : "SimpleOutput";

    return {config, section};
  }

  void SettingsManager::updateBufferLengthSettings(int seconds)
  {
    try
    {
      ConfigContext ctx = getConfigContext();

      if (config_get_uint(ctx.config, ctx.section, Config::REPLAY_BUFFER_LENGTH_KEY) == seconds)
      {
        return;
      }

      config_set_uint(ctx.config, ctx.section, Config::REPLAY_BUFFER_LENGTH_KEY, seconds);
      config_save(ctx.config);

      if (obs_output_t *replay_output = obs_frontend_get_replay_buffer_output())
      {
        OBSDataRAII settings(obs_output_get_settings(replay_output));
        if (settings.isValid())
        {
          obs_data_set_int(settings.get(), "max_time_sec", seconds);
          obs_output_update(replay_output, settings.get());
        }
        obs_output_release(replay_output);
      }

      obs_frontend_save();
      Logger::info("Updated buffer length to %d seconds", seconds);
    }
    catch (const std::exception &e)
    {
      Logger::error("Failed to update buffer length: %s", e.what());
      throw;
    }
  }

  int SettingsManager::getCurrentBufferLength()
  {
    ConfigContext ctx = getConfigContext();
    uint64_t currentBufferLength = config_get_uint(ctx.config, ctx.section, Config::REPLAY_BUFFER_LENGTH_KEY);

    return static_cast<int>(currentBufferLength);
  }

} // namespace ReplayBufferPro
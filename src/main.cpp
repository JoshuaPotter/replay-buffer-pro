// OBS includes
#include <obs-module.h>
#include <obs-frontend-api.h>

// Plugin includes
#include "plugin/plugin.hpp"
#include "utils/logger.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("replay-buffer-pro", "en-US")

namespace
{
  ReplayBufferPro::Plugin *pluginInstance = nullptr;
}

void obs_module_post_load(void)
{
  pluginInstance = new ReplayBufferPro::Plugin();

  obs_frontend_add_dock_by_id("replay-buffer-pro", "Replay Buffer Pro", pluginInstance);
}

bool obs_module_load(void)
{
  ReplayBufferPro::Logger::info("Plugin loaded");
  return true;
}

void obs_module_unload(void)
{
  // The dock widget will be automatically deleted by OBS
  pluginInstance = nullptr;
  ReplayBufferPro::Logger::info("Plugin unloaded");
}
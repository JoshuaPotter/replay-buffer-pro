#include "obs-module.h"
#include "obs-frontend-api.h"
#include "replay_buffer_pro.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("replay-buffer-pro", "en-US")

namespace {
    ReplayBufferPro* replayBufferProWindow = nullptr;
}

void obs_module_post_load(void) {
    auto mainWindow = static_cast<QMainWindow*>(obs_frontend_get_main_window());
    replayBufferProWindow = new ReplayBufferPro(mainWindow);
    
    obs_frontend_add_dock(replayBufferProWindow);
}

bool obs_module_load(void) {
    blog(LOG_INFO, "Replay Buffer Pro Plugin Loaded.");
    return true;
}

void obs_module_unload(void) {
    // The dock widget will be automatically deleted by OBS
    replayBufferProWindow = nullptr;
    blog(LOG_INFO, "Replay Buffer Pro Plugin Unloaded.");
}
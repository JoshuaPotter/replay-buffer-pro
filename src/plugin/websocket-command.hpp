#pragma once

namespace ReplayBufferPro
{
  class ReplayBufferManager;

  /**
   * @brief Registers the obs-websocket vendor request replay-buffer-pro/SaveClip
   * @param manager Manager that performs the save; must outlive the registration
   * @note Main thread only. Call from obs_module_post_load, after obs-websocket has loaded.
   */
  void registerSaveClipCommand(ReplayBufferManager *manager);

  /**
   * @brief Unregisters SaveClip and releases any request waiting on the main thread
   * @note Main thread only. Call before OBS tears down the frontend; safe to call twice.
   */
  void unregisterSaveClipCommand();
}

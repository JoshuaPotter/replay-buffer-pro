#include "plugin/websocket-command.hpp"
#include "managers/replay-buffer-manager.hpp"
#include "managers/settings-manager.hpp"
#include "config/config.hpp"
#include "utils/logger.hpp"

#include <QObject>
#include <QThread>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <memory>
#include <mutex>

// Vendor API ABI from obsproject/obs-websocket (GPL-2.0-or-later):
// https://github.com/obsproject/obs-websocket/blob/1f37e6179be80af6652d5ff97d3f91b8705da81a/lib/obs-websocket-api.h
// Copyright (C) 2016-2021 Stephane Lepin; 2020-2022 Kyle Manning.
struct obs_websocket_request_callback
{
  void (*callback)(obs_data_t *, obs_data_t *, void *);
  void *priv_data;
};

namespace ReplayBufferPro
{
  namespace
  {
    proc_handler_t *websocketApi = nullptr;
    void *vendor = nullptr;
    // Module lifetime: obs-websocket can copy a callback before unregistration.
    std::mutex commandMutex;
    std::condition_variable commandFinished;
    ReplayBufferManager *saveManager = nullptr;
    QObject *dispatchTarget = nullptr;

    // Main thread, with commandMutex held. Returns nullptr when the save was accepted,
    // otherwise a short reason; saveSegment itself only reports success or failure.
    // A request longer than the buffer saves the whole buffer instead of failing.
    const char *save(int requested, int &saved)
    {
      if (!obs_frontend_replay_buffer_active())
        return "buffer-inactive";
      saved = std::min(requested, SettingsManager().getCurrentBufferLength());
      return saveManager->saveSegment(saved, nullptr) ? nullptr : "save-refused";
    }

    void saveClip(obs_data_t *request, obs_data_t *response, void *)
    {
      obs_data_set_bool(response, "accepted", false);
      obs_data_item_t *item = obs_data_item_byname(request, "durationSeconds");
      const bool numeric = obs_data_item_gettype(item) == OBS_DATA_NUMBER;
      const double seconds = obs_data_item_get_double(item);
      obs_data_item_release(&item);
      if (!numeric || !std::isfinite(seconds) || seconds < 1 ||
          seconds > Config::MAX_BUFFER_LENGTH || std::floor(seconds) != seconds)
      {
        obs_data_set_string(response, "error", "invalid-duration");
        return;
      }

      struct Result { bool done = false; bool abandoned = false; const char *error = "unavailable"; int saved = 0; };
      auto result = std::make_shared<Result>();
      std::unique_lock<std::mutex> lock(commandMutex);
      if (!saveManager)
      {
        obs_data_set_string(response, "error", result->error);
        return;
      }

      const int duration = static_cast<int>(seconds);
      if (QThread::currentThread() == dispatchTarget->thread())
      {
        result->error = save(duration, result->saved);
        result->done = true;
      }
      else
      {
        // Wait only for entry into the existing save path, never for file I/O.
        // Unlike BlockingQueuedConnection, this wait can be released at EXIT,
        // when OBS may stop servicing Qt events before joining WebSocket threads.
        // It is also bounded: obs-websocket's settings dialog restarts the server
        // on the main thread and waits for this request, so an unbounded wait
        // would deadlock OBS. A timed-out request is abandoned and never saves.
        const bool queued = QMetaObject::invokeMethod(dispatchTarget, [result, duration]() {
          std::lock_guard<std::mutex> guard(commandMutex);
          if (saveManager && !result->abandoned)
            result->error = save(duration, result->saved);
          result->done = true;
          commandFinished.notify_all();
        }, Qt::QueuedConnection);
        if (queued && !commandFinished.wait_for(lock, std::chrono::seconds(2),
                                                [&]() { return result->done || !saveManager; }))
        {
          result->abandoned = true;
          result->error = "timeout";
        }
      }

      obs_data_set_bool(response, "accepted", !result->error);
      if (result->error)
      {
        obs_data_set_string(response, "error", result->error);
        return;
      }
      obs_data_set_int(response, "durationSeconds", result->saved);
      obs_data_set_bool(response, "clamped", result->saved < duration);
    }
  }

  void registerSaveClipCommand(ReplayBufferManager *manager)
  {
    calldata_t data = {};
    proc_handler_call(obs_get_proc_handler(), "obs_websocket_api_get_ph", &data);
    websocketApi = static_cast<proc_handler_t *>(calldata_ptr(&data, "ph"));
    calldata_free(&data);
    data = {};
    bool registered = false;
    if (websocketApi)
    {
      calldata_set_string(&data, "name", "replay-buffer-pro");
      proc_handler_call(websocketApi, "vendor_register", &data);
      vendor = calldata_ptr(&data, "vendor");
      if (vendor)
      {
        obs_websocket_request_callback callback = {saveClip, nullptr};
        calldata_set_string(&data, "type", "SaveClip");
        calldata_set_ptr(&data, "callback", &callback);
        calldata_set_bool(&data, "success", false);
        proc_handler_call(websocketApi, "vendor_request_register", &data);
        registered = calldata_bool(&data, "success");
      }
      calldata_free(&data);
    }
    if (registered)
    {
      std::lock_guard<std::mutex> lock(commandMutex);
      dispatchTarget = new QObject();
      saveManager = manager;
    }
    else
    {
      vendor = nullptr;
      Logger::warning("SaveClip unavailable: could not register obs-websocket vendor request");
    }
  }

  void unregisterSaveClipCommand()
  {
    QObject *target;
    {
      std::lock_guard<std::mutex> lock(commandMutex);
      saveManager = nullptr;
      target = dispatchTarget;
      dispatchTarget = nullptr;
    }
    commandFinished.notify_all();
    // Cancel queued functors while this module's code is still loaded.
    delete target;
    if (vendor)
    {
      calldata_t data = {};
      calldata_set_ptr(&data, "vendor", vendor);
      calldata_set_string(&data, "type", "SaveClip");
      proc_handler_call(websocketApi, "vendor_request_unregister", &data);
      calldata_free(&data);
      vendor = nullptr;
    }
  }
}

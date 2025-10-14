### Critical issues
- MacOS & Linux support

- Execute FFmpeg with mutable command buffer on Windows
  - Passing `command.c_str()` to `CreateProcessA` after casting away const is undefined behavior (Windows may modify the buffer).
  - Reference:
```122:151:src/managers/replay-buffer-manager.cpp
  bool ReplayBufferManager::executeFFmpegCommand(const std::string &command)
  {
#ifdef _WIN32
    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    // Create process
    if (!CreateProcessA(nullptr, (LPSTR)command.c_str(),
                        nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
    {
      return false;
    }
```
  - Fix:
```cpp
std::vector<char> cmdBuf(command.begin(), command.end());
cmdBuf.push_back('\0');
if (!CreateProcessA(nullptr, cmdBuf.data(),
                    nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
  return false;
}
```

- Blocking work in OBS event thread; potential UI thread usage
  - Trimming runs synchronously inside the OBS frontend event callback, potentially blocking OBS and showing a `QMessageBox` from a non-UI thread.
  - References:
```136:141:src/plugin/plugin.cpp
    case OBS_FRONTEND_EVENT_REPLAY_BUFFER_SAVED:
      plugin->handleReplayBufferSaved();
      break;
```
```198:209:src/plugin/plugin.cpp
  void Plugin::handleReplayBufferSaved() 
  {
    int duration = replayManager->getPendingSaveDuration();
    if (duration > 0) {
      const char* savedPath = obs_frontend_get_last_replay();
      if (savedPath) {
        replayManager->trimReplayBuffer(savedPath, duration);
        bfree((void*)savedPath);
      }
      replayManager->clearPendingSaveDuration();
    }
  }
```
  - Fix: move trimming to a background thread; copy the path before freeing:
```cpp
const char* savedPath = obs_frontend_get_last_replay();
if (savedPath) {
  std::string pathCopy(savedPath);
  bfree((void*)savedPath);
  std::thread([this, path = std::move(pathCopy), duration]{
    replayManager->trimReplayBuffer(path.c_str(), duration);
  }).detach();
}
```
  - Also avoid `QMessageBox` in `trimReplayBuffer` or emit a signal so the UI thread handles user prompts.

### Correctness and robustness
- Possible missing default when no OBS setting exists
  - If `RecRBTime` is not set, `getCurrentBufferLength` returns 0, and the UI is never initialized to a sensible value. This makes the slider/spin box inconsistent at startup.
  - References:
```69:75:src/managers/settings-manager.cpp
  int SettingsManager::getCurrentBufferLength()
  {
    ConfigContext ctx = getConfigContext();
    uint64_t currentBufferLength = config_get_uint(ctx.config, ctx.section, Config::REPLAY_BUFFER_LENGTH_KEY);

    return static_cast<int>(currentBufferLength);
  }
```
```225:233:src/plugin/plugin.cpp
  void Plugin::loadBufferLength()
  {
    int bufferLength = settingsManager->getCurrentBufferLength();
    if (bufferLength > 0 && bufferLength != lastKnownBufferLength)
    {
      lastKnownBufferLength = bufferLength;
      ui->updateBufferLengthValue(bufferLength);
    }
  }
```
  - Fix options:
    - On first load, if 0, set UI to `Config::DEFAULT_BUFFER_LENGTH` and write it to OBS.
    - Or change `loadBufferLength` to fall back to default when value <= 0.

- Exceptions unhandled in timer slot
  - `loadBufferLength` can throw (e.g., early OBS lifecycle) but is not guarded; a throw in a Qt slot can terminate the app.
  - Fix:
```cpp
try {
  int bufferLength = settingsManager->getCurrentBufferLength();
  ...
} catch (const std::exception& e) {
  Logger::error("loadBufferLength failed: %s", e.what());
}
```

- Replay duration > buffer length check duplicates UI gating
  - UI already disables save buttons above current buffer; the runtime guard is still good, but consider a clearer message or consistent behavior if the setting changes between UI and actual save.

### Resource management issues
- Leaks in plugin
  - `settingsManager` and `hotkeyManager` are allocated with `new` and not parented or deleted.
  - Reference:
```33:75:src/plugin/plugin.cpp
  Plugin::Plugin(QWidget *parent)
      : QWidget(parent), 
        lastKnownBufferLength(0)
  {
    ...
    replayManager = new ReplayBufferManager(this);
    settingsManager = new SettingsManager();
    ...
    hotkeyManager = new HotkeyManager(
      [this](int duration) { handleSaveSegment(duration); }
    );
    hotkeyManager->registerHotkeys();
```
```79:92:src/plugin/plugin.cpp
  Plugin::~Plugin()
  {
    ...
    obs_frontend_remove_event_callback(handleOBSEvent, this);
    // No need for explicit manager deletion since destructors are defaulted
    // Qt parent-child relationship will handle cleanup
  }
```
  - Fix: delete them in the destructor or give them ownership semantics (e.g., make `HotkeyManager` a `QObject` with parent).
```cpp
Plugin::~Plugin() {
  if (settingsMonitorTimer) settingsMonitorTimer->stop();
  obs_frontend_remove_event_callback(handleOBSEvent, this);
  delete hotkeyManager;
  delete settingsManager;
}
```

- UI event filter lifetime
  - `BufferLengthEventFilter` is allocated without a parent; leaks.
  - Reference:
```103:113:src/ui/ui-components.cpp
    secondsEdit->installEventFilter(new BufferLengthEventFilter());
...
    slider->installEventFilter(new BufferLengthEventFilter());
```
  - Fix:
```cpp
auto filter = new BufferLengthEventFilter(this);
secondsEdit->installEventFilter(filter);
slider->installEventFilter(filter);
```

### Maintainability and dead code
- Unused declaration in `HotkeyManager`
  - `handleHotkeyPress` is declared but not defined or used.
  - Reference:
```84:91:src/managers/hotkey-manager.hpp
    void handleHotkeyPress(obs_hotkey_id id);
```
  - Fix: remove it from the header 

### Minor functional gaps and polish
- Locale mismatch
  - `en-US.ini` contains `Save10Min`, but 10 minutes isn’t in `SAVE_BUTTONS`. Either add a 10-minute button or remove that key.

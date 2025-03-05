/**
 * @file hotkey-manager.cpp
 * @brief Implementation of hotkey management for the Replay Buffer Pro plugin
 */

#include "managers/hotkey-manager.hpp"
#include "utils/logger.hpp"

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
    for (size_t i = 0; i < SAVE_BUTTON_COUNT; i++) {
      saveHotkeys[i] = OBS_INVALID_HOTKEY_ID;
    }
  }

  HotkeyManager::~HotkeyManager()
  {
    // Unregister all hotkeys
    for (size_t i = 0; i < SAVE_BUTTON_COUNT; i++) {
      if (saveHotkeys[i] != OBS_INVALID_HOTKEY_ID) {
        obs_hotkey_unregister(saveHotkeys[i]);
      }
    }
    
    Logger::info("Hotkeys unregistered");
  }

  //=============================================================================
  // HOTKEY MANAGEMENT
  //=============================================================================

  void HotkeyManager::registerHotkeys()
  {
    // Register hotkeys for each save duration
    for (size_t i = 0; i < SAVE_BUTTON_COUNT; i++) {
      const auto &btn = SAVE_BUTTONS[i];
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
            for (size_t i = 0; i < SAVE_BUTTON_COUNT; i++) {
              if (self->saveHotkeys[i] == id) {
                duration = SAVE_BUTTONS[i].duration;
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
  }

} // namespace ReplayBufferPro 
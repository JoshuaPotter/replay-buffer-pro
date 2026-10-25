/**
 * @file save-button-settings.hpp
 * @brief Global settings for save buttons and other plugin properties
 */

#pragma once

// STL includes
#include <vector>
#include <string>

namespace ReplayBufferPro
{
  /**
   * @brief One dock save button
   */
  struct SaveButton
  {
    int seconds;       ///< Clip length
    std::string label; ///< Custom text; empty means "Last <duration>"
  };

  class SaveButtonSettings
  {
  public:
    SaveButtonSettings();

    const std::vector<SaveButton> &getButtons() const;

    /**
     * @brief Replaces the buttons, keeping 1..MAX_SAVE_BUTTON_COUNT and clamping durations
     */
    void setButtons(const std::vector<SaveButton> &buttons);

    bool getInMemoryClips() const;
    void setInMemoryClips(bool enabled);

    void load();
    bool save() const;

    static std::vector<SaveButton> getDefaultButtons();

  private:
    std::vector<SaveButton> buttons;
    bool inMemoryClips = false;

    std::vector<SaveButton> normalizeButtons(const std::vector<SaveButton> &input) const;
    std::string getConfigPath() const;
  };
} // namespace ReplayBufferPro

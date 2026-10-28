/**
 * @file plugin-properties.hpp
 * @brief Global plugin properties: save buttons, experimental options, window size
 */

#pragma once

// OBS includes
#include <obs-data.h>

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

  /**
   * @brief Everything the Properties window edits, stored in properties.json
   *        under the module config path
   */
  class PluginProperties
  {
  public:
    PluginProperties();

    const std::vector<SaveButton> &getButtons() const;

    /**
     * @brief Replaces the buttons, keeping 1..MAX_SAVE_BUTTON_COUNT and clamping durations
     */
    void setButtons(const std::vector<SaveButton> &buttons);

    bool getInMemoryClips() const;
    void setInMemoryClips(bool enabled);

    /**
     * @brief Size the user last gave the Properties window (0 when never resized)
     */
    int getWindowWidth() const;
    int getWindowHeight() const;
    void setWindowSize(int width, int height);

    /**
     * @brief Loads properties.json, or save_button_settings.json from older
     *        versions when properties.json does not exist yet
     */
    void load();
    bool save() const;

    static std::vector<SaveButton> getDefaultButtons();

  private:
    std::vector<SaveButton> buttons;
    bool inMemoryClips = false;
    int windowWidth = 0;
    int windowHeight = 0;

    void loadFrom(obs_data_t *data);
    std::vector<SaveButton> normalizeButtons(const std::vector<SaveButton> &input) const;
  };
} // namespace ReplayBufferPro

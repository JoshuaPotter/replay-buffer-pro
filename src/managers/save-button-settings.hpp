/**
 * @file save-button-settings.hpp
 * @brief Global settings for save button durations
 */

#pragma once

// STL includes
#include <vector>
#include <string>

namespace ReplayBufferPro
{
  class SaveButtonSettings
  {
  public:
    SaveButtonSettings();

    const std::vector<int> &getDurations() const;
    void setDurations(const std::vector<int> &durations);

    void load();
    bool save() const;

    static std::vector<int> getDefaultDurations();

  private:
    std::vector<int> durations;

    std::vector<int> normalizeDurations(const std::vector<int> &input) const;
    std::string getConfigPath() const;
  };
} // namespace ReplayBufferPro

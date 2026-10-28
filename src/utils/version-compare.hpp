#pragma once

#include <optional>
#include <string_view>

namespace ReplayBufferPro
{
  /**
   * @brief Compares two dotted release versions ("1.9.0", "v1.10.0")
   * @return Negative if a is older than b, 0 if they are the same release,
   *         positive if a is newer, or nullopt when either is not a version
   * @note A leading "v" is ignored and missing parts count as 0, so "1.9"
   *       equals "1.9.0". Pre-release suffixes are not versions. Pure C++ so
   *       it can be tested without OBS or Qt.
   */
  std::optional<int> compareVersions(std::string_view a, std::string_view b);
}

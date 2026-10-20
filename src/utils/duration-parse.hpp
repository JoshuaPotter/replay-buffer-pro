#pragma once

#include <optional>
#include <string_view>

namespace ReplayBufferPro
{
  /**
   * @brief Parses a duration written as seconds, a clock time or units
   * @param text Seconds ("120"), m:ss or h:mm:ss ("5:22.6", "1:05:00"), or units
   *             largest first ("90s", "1.5h", "1h30m", "5m 22s"); case and
   *             surrounding whitespace are ignored
   * @return Seconds rounded to the nearest whole second (halves up), or nullopt
   *         when the text is not a duration
   * @note Does not check the plugin's duration range. Pure C++ so it can be
   *       tested without OBS or Qt.
   */
  std::optional<int> parseDurationSeconds(std::string_view text);
}

#pragma once

// Qt includes
#include <QString>

// STL includes
#include <string>

namespace ReplayBufferPro
{
  QString formatDurationValue(int seconds);
  QString formatDurationLabel(int seconds);
  QString formatHotkeyDescription(int seconds);

  /**
   * @brief Dock button text: the custom label, or "Last <duration>" when it is empty
   */
  QString formatButtonLabel(int seconds, const std::string &label);

  /**
   * @brief Hotkey name: "Replay Buffer Pro: <label>", or "...: Save <duration>" when the label is empty
   */
  QString formatButtonHotkeyDescription(int seconds, const std::string &label);
} // namespace ReplayBufferPro

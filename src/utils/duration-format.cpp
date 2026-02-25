/**
 * @file duration-format.cpp
 * @brief Localized duration formatting helpers
 */

#include "utils/duration-format.hpp"

// OBS includes
#include <obs-module.h>

// STL includes
#include <algorithm>

namespace ReplayBufferPro
{
  namespace
  {
    QString formatUnitLabel(int value, const char *singularKey, const char *pluralKey)
    {
      const char *key = (value == 1) ? singularKey : pluralKey;
      return QString::fromUtf8(obs_module_text(key));
    }
  } // namespace

  QString formatDurationValue(int seconds)
  {
    int clampedSeconds = std::max(1, seconds);
    int value = clampedSeconds;
    QString unitLabel;

    if (clampedSeconds >= 3600 && clampedSeconds % 3600 == 0)
    {
      value = clampedSeconds / 3600;
      unitLabel = formatUnitLabel(value, "TimeUnitHour", "TimeUnitHours");
    }
    else if (clampedSeconds >= 60 && clampedSeconds % 60 == 0)
    {
      value = clampedSeconds / 60;
      unitLabel = formatUnitLabel(value, "TimeUnitMinute", "TimeUnitMinutes");
    }
    else
    {
      unitLabel = formatUnitLabel(value, "TimeUnitSecond", "TimeUnitSeconds");
    }

    return QString("%1 %2").arg(value).arg(unitLabel);
  }

  QString formatDurationLabel(int seconds)
  {
    QString templateText = QString::fromUtf8(obs_module_text("SaveClipTemplate"));
    return templateText.arg(formatDurationValue(seconds));
  }

  QString formatHotkeyDescription(int seconds)
  {
    QString templateText = QString::fromUtf8(obs_module_text("SaveClipHotkeyTemplate"));
    return templateText.arg(formatDurationValue(seconds));
  }
} // namespace ReplayBufferPro

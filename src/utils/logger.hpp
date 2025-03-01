#pragma once

#include <obs.h>
#include <cstdarg>

namespace ReplayBufferPro
{

  /**
   * @brief Logger for standardized OBS logging
   */
  class Logger
  {
  public:
    /**
     * @brief Log info message
     * @param format Format string
     * @param ... Format arguments
     */
    static void info(const char *format, ...)
    {
      va_list args;
      va_start(args, format);
      char buf[4096];
      vsnprintf(buf, sizeof(buf), format, args);
      blog(LOG_INFO, "[ReplayBufferPro] %s", buf);
      va_end(args);
    }

    /**
     * @brief Log error message
     * @param format Format string
     * @param ... Format arguments
     */
    static void error(const char *format, ...)
    {
      va_list args;
      va_start(args, format);
      char buf[4096];
      vsnprintf(buf, sizeof(buf), format, args);
      blog(LOG_ERROR, "[ReplayBufferPro] %s", buf);
      va_end(args);
    }
  };

} // namespace ReplayBufferPro
#pragma once

#include <obs.h>
#include <cstdarg>

namespace ReplayBufferPro {

/**
 * @brief Utility class for standardized logging
 */
class Logger {
public:
    /**
     * @brief Log an informational message
     * @param format Printf-style format string
     * @param ... Variable arguments for format string
     */
    static void info(const char* format, ...) {
        va_list args;
        va_start(args, format);
        char buf[4096];
        vsnprintf(buf, sizeof(buf), format, args);
        blog(LOG_INFO, "[ReplayBufferPro] %s", buf);
        va_end(args);
    }

    /**
     * @brief Log an error message
     * @param format Printf-style format string
     * @param ... Variable arguments for format string
     */
    static void error(const char* format, ...) {
        va_list args;
        va_start(args, format);
        char buf[4096];
        vsnprintf(buf, sizeof(buf), format, args);
        blog(LOG_ERROR, "[ReplayBufferPro] %s", buf);
        va_end(args);
    }
};

} // namespace ReplayBufferPro 
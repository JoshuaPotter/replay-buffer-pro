/**
 * @file libav-utils.hpp
 * @brief Small libavformat helpers shared by the trimmer and the clip writer
 * @author Joshua Potter
 * @copyright GPL v2 or later
 */

#pragma once

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/error.h>
}

#include <string>

namespace ReplayBufferPro {

/**
 * @brief Converts a libav error code to text (MSVC-compatible av_err2str)
 */
inline std::string avErrorString(int errnum) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(errnum, errbuf, AV_ERROR_MAX_STRING_SIZE);
    return std::string(errbuf);
}

/**
 * @brief Frees an output context and its AVIO handle
 */
inline void closeOutput(AVFormatContext **outputCtx) {
    if (!*outputCtx) {
        return;
    }
    if ((*outputCtx)->pb && !((*outputCtx)->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&(*outputCtx)->pb);
    }
    avformat_free_context(*outputCtx);
    *outputCtx = nullptr;
}

} // namespace ReplayBufferPro

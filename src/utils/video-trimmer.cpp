/**
 * @file video-trimmer.cpp
 * @brief Implementation of video trimming using libavformat
 * @author Joshua Potter
 * @copyright GPL v2 or later
 *
 * This file implements video trimming functionality using FFmpeg's libavformat
 * library instead of external ffmpeg binary execution.
 */

#include "video-trimmer.hpp"
#include "logger.hpp"
#include "config/config.hpp"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/timestamp.h>
#include <libavutil/mathematics.h>
#include <libavutil/error.h>
#include <libavutil/log.h>
}

// OBS includes
#include <util/platform.h>

// Helper function to convert error codes to strings (MSVC-compatible)
static std::string av_error_string(int errnum) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(errnum, errbuf, AV_ERROR_MAX_STRING_SIZE);
    return std::string(errbuf);
}

#include <algorithm>
#include <cmath>
#include <vector>

namespace ReplayBufferPro {

namespace {

/**
 * @brief Best available presentation time for a packet, in seconds
 *
 * Prefers DTS because it is monotonic across B-frames, which PTS is not.
 * Returns false when the packet carries no usable timestamp at all.
 */
bool packetTimeSeconds(const AVPacket* packet, const AVStream* stream, double* out) {
    if (packet->dts != AV_NOPTS_VALUE) {
        *out = static_cast<double>(packet->dts) * av_q2d(stream->time_base);
        return true;
    }
    if (packet->pts != AV_NOPTS_VALUE) {
        *out = static_cast<double>(packet->pts) * av_q2d(stream->time_base);
        return true;
    }
    return false;
}

/**
 * @brief Frees an output context and its AVIO handle
 */
void closeOutput(AVFormatContext** outputCtx) {
    if (!*outputCtx) {
        return;
    }
    if ((*outputCtx)->pb && !((*outputCtx)->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&(*outputCtx)->pb);
    }
    avformat_free_context(*outputCtx);
    *outputCtx = nullptr;
}

/**
 * @brief Builds a failed TrimResult carrying the diagnostics gathered so far
 */
TrimResult failure(TrimResult result, std::string reason, std::string detail = {}) {
    result.success = false;
    result.reason = std::move(reason);
    result.detail = std::move(detail);
    return result;
}

} // namespace

bool VideoTrimmer::openInputWithRetry(const std::string& inputPath,
                                      AVFormatContext** inputCtx,
                                      int* lastError) {
    int delayMs = Config::TRIM_OPEN_RETRY_DELAY_MS;

    for (int attempt = 1; attempt <= Config::TRIM_OPEN_RETRY_COUNT; attempt++) {
        int ret = avformat_open_input(inputCtx, inputPath.c_str(), nullptr, nullptr);
        if (ret >= 0) {
            if (attempt > 1) {
                Logger::info("Opened input on attempt %d of %d",
                             attempt, Config::TRIM_OPEN_RETRY_COUNT);
            }
            return true;
        }

        *lastError = ret;
        *inputCtx = nullptr;

        if (attempt < Config::TRIM_OPEN_RETRY_COUNT) {
            Logger::warning("Could not open '%s' (attempt %d of %d): %s - retrying in %dms",
                            inputPath.c_str(), attempt, Config::TRIM_OPEN_RETRY_COUNT,
                            av_error_string(ret).c_str(), delayMs);
            os_sleep_ms(delayMs);
            delayMs *= 2;
        }
    }

    return false;
}

TrimResult VideoTrimmer::trimToLastSeconds(const std::string& inputPath,
                                           const std::string& outputPath,
                                           int durationSeconds) {
    initializeFFmpeg();

    TrimResult result;
    AVFormatContext* inputCtx = nullptr;
    AVFormatContext* outputCtx = nullptr;

    try {
        Logger::info("Starting video trim operation: %s -> %s (%d seconds)",
                    inputPath.c_str(), outputPath.c_str(), durationSeconds);

        // Open input file, tolerating a file that is briefly locked
        int openError = 0;
        if (!openInputWithRetry(inputPath, &inputCtx, &openError)) {
            Logger::error("Could not open input file '%s': %s",
                         inputPath.c_str(), av_error_string(openError).c_str());
            return failure(result, "open-input-failed", av_error_string(openError));
        }

        // Retrieve stream information
        int ret = avformat_find_stream_info(inputCtx, nullptr);
        if (ret < 0) {
            Logger::error("Could not find stream information: %s", av_error_string(ret).c_str());
            avformat_close_input(&inputCtx);
            return failure(result, "stream-info-failed", av_error_string(ret));
        }

        // Get total duration
        double totalDuration = getVideoDuration(inputPath, inputCtx);
        if (totalDuration <= 0) {
            Logger::error("Could not determine video duration or file is empty");
            avformat_close_input(&inputCtx);
            return failure(result, "duration-unavailable");
        }

        result.sourceDuration = totalDuration;
        Logger::info("Input video duration: %.2f seconds", totalDuration);

        // Calculate start time (total duration - desired duration)
        // Ensure we don't go before the beginning of the file
        double startTime = std::max(0.0, totalDuration - durationSeconds);
        result.requestedStart = startTime;

        Logger::info("Trimming from %.2f seconds to end (%.2f seconds total)",
                    startTime, totalDuration - startTime);

        // Create output context
        ret = avformat_alloc_output_context2(&outputCtx, nullptr, nullptr, outputPath.c_str());
        if (ret < 0) {
            Logger::error("Could not create output context: %s", av_error_string(ret).c_str());
            avformat_close_input(&inputCtx);
            return failure(result, "output-context-failed", av_error_string(ret));
        }

        // Setup output streams to match input
        if (!setupOutputStreams(inputCtx, outputCtx)) {
            Logger::error("Failed to setup output streams");
            avformat_close_input(&inputCtx);
            closeOutput(&outputCtx);
            return failure(result, "output-streams-failed");
        }

        // Open output file
        if (!(outputCtx->oformat->flags & AVFMT_NOFILE)) {
            ret = avio_open(&outputCtx->pb, outputPath.c_str(), AVIO_FLAG_WRITE);
            if (ret < 0) {
                Logger::error("Could not open output file '%s': %s",
                             outputPath.c_str(), av_error_string(ret).c_str());
                avformat_close_input(&inputCtx);
                closeOutput(&outputCtx);
                return failure(result, "output-open-failed", av_error_string(ret));
            }
        }

        // Write header
        ret = avformat_write_header(outputCtx, nullptr);
        if (ret < 0) {
            Logger::error("Error occurred when writing header: %s", av_error_string(ret).c_str());
            avformat_close_input(&inputCtx);
            closeOutput(&outputCtx);
            return failure(result, "write-header-failed", av_error_string(ret));
        }

        // Find the video stream
        int videoStreamIndex = -1;
        for (unsigned int i = 0; i < inputCtx->nb_streams; i++) {
            if (inputCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                videoStreamIndex = static_cast<int>(i);
                break;
            }
        }

        // Establish the cut point: the last keyframe at or before the requested
        // start. Timestamps are compared as DTS, which is monotonic; comparing PTS
        // here lets a reordered B-frame end the search early and drag the cut
        // backwards by a whole GOP.
        double cutTime = startTime;
        auto seekToStart = [&](void) -> int {
            if (videoStreamIndex >= 0) {
                int64_t target = av_rescale_q(static_cast<int64_t>(startTime * AV_TIME_BASE),
                                              AV_TIME_BASE_Q,
                                              inputCtx->streams[videoStreamIndex]->time_base);
                return av_seek_frame(inputCtx, videoStreamIndex, target, AVSEEK_FLAG_BACKWARD);
            }
            return av_seek_frame(inputCtx, -1, static_cast<int64_t>(startTime * AV_TIME_BASE),
                                 AVSEEK_FLAG_BACKWARD);
        };

        ret = seekToStart();
        const bool seekSucceeded = (ret >= 0);
        if (!seekSucceeded) {
            // Not fatal, but now the scan starts at the head of the file, so it has
            // to keep going until it passes startTime rather than trusting the seek.
            Logger::warning("Error seeking to start time %.2f: %s - scanning from the beginning",
                            startTime, av_error_string(ret).c_str());
        }

        if (videoStreamIndex >= 0) {
            AVPacket* searchPacket = av_packet_alloc();
            if (!searchPacket) {
                Logger::error("Could not allocate search packet");
                avformat_close_input(&inputCtx);
                closeOutput(&outputCtx);
                return failure(result, "packet-alloc-failed");
            }

            bool foundKeyframe = false;
            while (av_read_frame(inputCtx, searchPacket) >= 0) {
                if (searchPacket->stream_index != videoStreamIndex) {
                    av_packet_unref(searchPacket);
                    continue;
                }

                double packetTime = 0.0;
                if (!packetTimeSeconds(searchPacket, inputCtx->streams[videoStreamIndex],
                                       &packetTime)) {
                    av_packet_unref(searchPacket);
                    continue;
                }

                if ((searchPacket->flags & AV_PKT_FLAG_KEY) && packetTime <= startTime) {
                    cutTime = packetTime;
                    foundKeyframe = true;

                    // A backward seek already lands on the closest keyframe at or
                    // before the target, so there is nothing better further on.
                    if (seekSucceeded) {
                        av_packet_unref(searchPacket);
                        break;
                    }
                }

                if (packetTime > startTime) {
                    av_packet_unref(searchPacket);
                    break;
                }

                av_packet_unref(searchPacket);
            }
            av_packet_free(&searchPacket);

            if (foundKeyframe) {
                double drift = startTime - cutTime;
                Logger::info("Cutting at keyframe %.2f seconds (requested %.2f, drift %.2f)",
                             cutTime, startTime, drift);
                if (drift > Config::TRIM_KEYFRAME_TOLERANCE_SECONDS) {
                    Logger::warning(
                        "Keyframe is %.2f seconds before the requested cut; the clip will be "
                        "that much longer than asked. Lower the encoder keyframe interval to "
                        "tighten this.",
                        drift);
                }
            } else {
                Logger::warning("No keyframe found at or before %.2f seconds; "
                                "cutting at the requested time instead", startTime);
            }

            // Rewind so the packets consumed by the search get copied. If seeking is
            // unavailable the scan has already run past the cut point, so fall back
            // to rewinding to the head of the file and letting the copy loop skip.
            ret = seekToStart();
            if (ret < 0) {
                Logger::warning("Could not rewind to the cut point (%s); restarting from "
                                "the beginning of the file",
                                av_error_string(ret).c_str());
                ret = av_seek_frame(inputCtx, -1, 0, AVSEEK_FLAG_BACKWARD);
                if (ret < 0) {
                    Logger::error("Could not rewind the input at all: %s",
                                  av_error_string(ret).c_str());
                    avformat_close_input(&inputCtx);
                    closeOutput(&outputCtx);
                    return failure(result, "rewind-failed", av_error_string(ret));
                }
            }
        }

        result.cutAt = cutTime;

        // Copy packets from the cut point to the end
        {
            AVPacket* packet = av_packet_alloc();
            if (!packet) {
                Logger::error("Could not allocate packet");
                avformat_close_input(&inputCtx);
                closeOutput(&outputCtx);
                return failure(result, "packet-alloc-failed");
            }

            // Every stream is rebased by the same wall-clock offset so they stay in
            // sync. Filtering on DTS guarantees the shifted timestamps stay positive,
            // because DTS <= PTS holds for every valid packet.
            std::vector<bool> streamStarted(inputCtx->nb_streams, false);
            std::vector<int64_t> lastDts(inputCtx->nb_streams, AV_NOPTS_VALUE);

            while (av_read_frame(inputCtx, packet) >= 0) {
                AVStream* inputStream = inputCtx->streams[packet->stream_index];
                AVStream* outputStream = outputCtx->streams[packet->stream_index];
                const int streamIndex = packet->stream_index;

                double packetTime = 0.0;
                if (packetTimeSeconds(packet, inputStream, &packetTime)) {
                    if (packetTime < cutTime) {
                        av_packet_unref(packet);
                        continue;
                    }
                } else if (!streamStarted[streamIndex]) {
                    // No timestamp and nothing copied for this stream yet, so there is
                    // no way to tell whether it belongs in the clip. Dropping it once
                    // the stream has started would punch a hole in the output.
                    av_packet_unref(packet);
                    continue;
                }

                streamStarted[streamIndex] = true;

                const int64_t offset = av_rescale_q(static_cast<int64_t>(cutTime * AV_TIME_BASE),
                                                    AV_TIME_BASE_Q, inputStream->time_base);

                if (packet->pts != AV_NOPTS_VALUE) {
                    packet->pts = av_rescale_q(packet->pts - offset,
                                               inputStream->time_base, outputStream->time_base);
                }
                if (packet->dts != AV_NOPTS_VALUE) {
                    packet->dts = av_rescale_q(packet->dts - offset,
                                               inputStream->time_base, outputStream->time_base);
                }
                if (packet->duration > 0) {
                    packet->duration = av_rescale_q(packet->duration,
                                                    inputStream->time_base, outputStream->time_base);
                }

                // A packet missing only one of the two timestamps is left alone: the
                // muxer infers the rest, and substituting PTS for a missing DTS
                // reorders B-frames badly enough that the write fails outright.
                // A packet missing both carries no ordering information at all, so
                // step it on from the previous one rather than dropping the frame.
                if (packet->pts == AV_NOPTS_VALUE && packet->dts == AV_NOPTS_VALUE &&
                    lastDts[streamIndex] != AV_NOPTS_VALUE) {
                    packet->dts = lastDts[streamIndex] + (packet->duration > 0 ? packet->duration : 1);
                    packet->pts = packet->dts;
                }

                if (packet->dts != AV_NOPTS_VALUE) {
                    lastDts[streamIndex] = packet->dts;
                }

                packet->pos = -1;

                ret = av_interleaved_write_frame(outputCtx, packet);
                if (ret < 0) {
                    Logger::error("Error writing packet: %s", av_error_string(ret).c_str());
                    av_packet_free(&packet);
                    avformat_close_input(&inputCtx);
                    closeOutput(&outputCtx);
                    return failure(result, "write-packet-failed", av_error_string(ret));
                }

                result.packetsWritten++;
                av_packet_unref(packet);
            }

            av_packet_free(&packet);
        }

        if (result.packetsWritten == 0) {
            // Writing a trailer here would produce a valid but empty file, and the
            // caller would then delete a perfectly good original in exchange for it.
            Logger::error("No packets were copied; refusing to write an empty clip");
            avformat_close_input(&inputCtx);
            closeOutput(&outputCtx);
            return failure(result, "no-packets-written");
        }

        // Write trailer
        ret = av_write_trailer(outputCtx);
        if (ret < 0) {
            Logger::error("Error writing trailer: %s", av_error_string(ret).c_str());
            avformat_close_input(&inputCtx);
            closeOutput(&outputCtx);
            return failure(result, "write-trailer-failed", av_error_string(ret));
        }

        avformat_close_input(&inputCtx);
        closeOutput(&outputCtx);

        Logger::info("Copied %lld packets covering %.2f seconds",
                     static_cast<long long>(result.packetsWritten), totalDuration - cutTime);

        result.success = true;
        return result;

    } catch (const std::exception& e) {
        Logger::error("Exception in video trimming: %s", e.what());
        if (inputCtx) {
            avformat_close_input(&inputCtx);
        }
        closeOutput(&outputCtx);
        return failure(result, "exception", e.what());
    }
}

void VideoTrimmer::initializeFFmpeg() {
    static bool initialized = false;
    if (!initialized) {
        // Note: In FFmpeg 4.0+, av_register_all() is deprecated and not needed
        // The libraries auto-register themselves
        Logger::info("FFmpeg libraries initialized for video trimming");
        initialized = true;
    }
}

double VideoTrimmer::getVideoDuration(const std::string& inputPath, AVFormatContext* inputCtx) {
    // If a context is already open, read duration from it directly instead of
    // reopening the file
    AVFormatContext* ctx = inputCtx;
    bool shouldClose = false;

    if (!ctx) {
        int ret = avformat_open_input(&ctx, inputPath.c_str(), nullptr, nullptr);
        if (ret < 0) {
            Logger::error("Could not open file for duration check: %s", av_error_string(ret).c_str());
            return -1.0;
        }

        ret = avformat_find_stream_info(ctx, nullptr);
        if (ret < 0) {
            Logger::error("Could not find stream info for duration check: %s", av_error_string(ret).c_str());
            avformat_close_input(&ctx);
            return -1.0;
        }
        shouldClose = true;
    }

    double duration = 0.0;
    if (ctx->duration != AV_NOPTS_VALUE) {
        duration = static_cast<double>(ctx->duration) / AV_TIME_BASE;
    } else {
        // Try to get duration from the longest stream
        for (unsigned int i = 0; i < ctx->nb_streams; i++) {
            AVStream* stream = ctx->streams[i];
            if (stream->duration != AV_NOPTS_VALUE) {
                double streamDuration = static_cast<double>(stream->duration) * av_q2d(stream->time_base);
                duration = std::max(duration, streamDuration);
            }
        }
    }

    if (shouldClose) {
        avformat_close_input(&ctx);
    }
    return duration;
}

bool VideoTrimmer::setupOutputStreams(AVFormatContext* inputCtx,
                                     AVFormatContext* outputCtx) {
    // Copy all streams from input to output
    for (unsigned int i = 0; i < inputCtx->nb_streams; i++) {
        AVStream* inputStream = inputCtx->streams[i];
        AVStream* outputStream = avformat_new_stream(outputCtx, nullptr);

        if (!outputStream) {
            Logger::error("Failed to allocate output stream %d", i);
            return false;
        }

        // Copy codec parameters
        int ret = avcodec_parameters_copy(outputStream->codecpar, inputStream->codecpar);
        if (ret < 0) {
            Logger::error("Failed to copy codec parameters for stream %d: %s", i, av_error_string(ret).c_str());
            return false;
        }

        // Clear codec tag to avoid issues with different containers
        outputStream->codecpar->codec_tag = 0;

        // Copy time base
        outputStream->time_base = inputStream->time_base;

        // Preserve stream metadata and disposition flags
        av_dict_copy(&outputStream->metadata, inputStream->metadata, 0);
        outputStream->disposition = inputStream->disposition;

        Logger::info("Setup output stream %d: codec=%s, time_base=%d/%d",
                    i, avcodec_get_name(outputStream->codecpar->codec_id),
                    outputStream->time_base.num, outputStream->time_base.den);
    }

    return true;
}

} // namespace ReplayBufferPro


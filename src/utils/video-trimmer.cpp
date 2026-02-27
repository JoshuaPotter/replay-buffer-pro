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

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/timestamp.h>
#include <libavutil/mathematics.h>
#include <libavutil/error.h>
#include <libavutil/log.h>
}

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

bool VideoTrimmer::trimToLastSeconds(const std::string& inputPath,
                                   const std::string& outputPath,
                                   int durationSeconds) {
    initializeFFmpeg();
    
    AVFormatContext* inputCtx = nullptr;
    AVFormatContext* outputCtx = nullptr;
    
    // Declare variables before any goto statements to avoid C2362 errors
    int videoStreamIndex = -1;
    int64_t keyframeTime = AV_NOPTS_VALUE;
    int64_t seekTarget = 0;
    double effectiveStartTime = 0.0;
    
    try {
        Logger::info("Starting video trim operation: %s -> %s (%d seconds)", 
                    inputPath.c_str(), outputPath.c_str(), durationSeconds);
        
        // Open input file
        int ret = avformat_open_input(&inputCtx, inputPath.c_str(), nullptr, nullptr);
        if (ret < 0) {
            Logger::error("Could not open input file '%s': %s", 
                         inputPath.c_str(), av_error_string(ret).c_str());
            return false;
        }
        
        // Retrieve stream information
        ret = avformat_find_stream_info(inputCtx, nullptr);
        if (ret < 0) {
            Logger::error("Could not find stream information: %s", av_error_string(ret).c_str());
            avformat_close_input(&inputCtx);
            return false;
        }
        
        // Get total duration (prefer input context if available)
        double totalDuration = -1.0;
        if (inputCtx->duration != AV_NOPTS_VALUE) {
            totalDuration = static_cast<double>(inputCtx->duration) / AV_TIME_BASE;
        } else {
            // Try to get duration from the longest stream
            for (unsigned int i = 0; i < inputCtx->nb_streams; i++) {
                AVStream* stream = inputCtx->streams[i];
                if (stream->duration != AV_NOPTS_VALUE) {
                    double streamDuration = static_cast<double>(stream->duration) * av_q2d(stream->time_base);
                    totalDuration = std::max(totalDuration, streamDuration);
                }
            }
        }

        if (totalDuration <= 0) {
            Logger::warning("Input context duration unavailable, falling back to duration probe");
            totalDuration = getVideoDuration(inputPath, inputCtx);
        }
        if (totalDuration <= 0) {
            Logger::error("Could not determine video duration or file is empty");
            avformat_close_input(&inputCtx);
            return false;
        }
        
        Logger::info("Input video duration: %.2f seconds", totalDuration);
        
        // Calculate start time (total duration - desired duration)
        // Ensure we don't go before the beginning of the file
        double startTime = std::max(0.0, totalDuration - durationSeconds);
        
        Logger::info("Trimming from %.2f seconds to end (%.2f seconds total)", 
                    startTime, totalDuration - startTime);
        
        // Create output context
        ret = avformat_alloc_output_context2(&outputCtx, nullptr, nullptr, outputPath.c_str());
        if (ret < 0) {
            Logger::error("Could not create output context: %s", av_error_string(ret).c_str());
            avformat_close_input(&inputCtx);
            return false;
        }
        
        // Setup output streams to match input
        if (!setupOutputStreams(inputCtx, outputCtx)) {
            Logger::error("Failed to setup output streams");
            goto cleanup;
        }
        
        // Open output file
        if (!(outputCtx->oformat->flags & AVFMT_NOFILE)) {
            ret = avio_open(&outputCtx->pb, outputPath.c_str(), AVIO_FLAG_WRITE);
            if (ret < 0) {
                Logger::error("Could not open output file '%s': %s", 
                             outputPath.c_str(), av_error_string(ret).c_str());
                goto cleanup;
            }
        }
        
        // Write header
        ret = avformat_write_header(outputCtx, nullptr);
        if (ret < 0) {
            Logger::error("Error occurred when writing header: %s", av_error_string(ret).c_str());
            goto cleanup;
        }
        
        // Find the video stream
        for (unsigned int i = 0; i < inputCtx->nb_streams; i++) {
            if (inputCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                videoStreamIndex = i;
                break;
            }
        }
        
        // Seek to start time
        seekTarget = static_cast<int64_t>(startTime * AV_TIME_BASE);
        ret = av_seek_frame(inputCtx, -1, seekTarget, AVSEEK_FLAG_BACKWARD);
        if (ret < 0) {
            Logger::error("Error seeking to start time %.2f: %s", startTime, av_error_string(ret).c_str());
            // Continue anyway - we might still be able to copy from the beginning
        }
        
        // Find the last keyframe at or before our desired start time.
        // AVSEEK_FLAG_BACKWARD already positioned the stream at or before startTime, so we
        // scan forward and keep updating keyframeTime for every key video frame we see until
        // we pass startTime. The last recorded keyframe is the correct cut point.
        effectiveStartTime = startTime;
        if (videoStreamIndex >= 0) {
            AVPacket* searchPacket = av_packet_alloc();
            if (searchPacket) {
                while (av_read_frame(inputCtx, searchPacket) >= 0) {
                    if (searchPacket->stream_index == videoStreamIndex) {
                        double packetTime = 0.0;
                        if (searchPacket->pts != AV_NOPTS_VALUE) {
                            packetTime = static_cast<double>(searchPacket->pts) *
                                       av_q2d(inputCtx->streams[videoStreamIndex]->time_base);
                        }

                        if (searchPacket->flags & AV_PKT_FLAG_KEY) {
                            // Keep tracking keyframes until we pass the cut point
                            keyframeTime = searchPacket->pts;
                            effectiveStartTime = packetTime;
                        }

                        // Once we've moved past startTime we have the last keyframe before it
                        if (packetTime > startTime) {
                            break;
                        }
                    }
                    av_packet_unref(searchPacket);
                }
                av_packet_free(&searchPacket);

                // Seek exactly to the chosen keyframe so all streams start from there
                if (keyframeTime != AV_NOPTS_VALUE) {
                    int64_t keyframeSeekTarget = av_rescale_q(keyframeTime,
                        inputCtx->streams[videoStreamIndex]->time_base, AV_TIME_BASE_Q);
                    // Use AVSEEK_FLAG_ANY (exact) — we already know this is a keyframe position
                    ret = av_seek_frame(inputCtx, -1, keyframeSeekTarget, AVSEEK_FLAG_ANY);
                    if (ret < 0) {
                        Logger::warning("Exact seek to keyframe failed, retrying with backward seek: %s",
                                       av_error_string(ret).c_str());
                        ret = av_seek_frame(inputCtx, -1, keyframeSeekTarget, AVSEEK_FLAG_BACKWARD);
                    }
                    Logger::info("Found keyframe at %.2f seconds (requested %.2f); all streams start here",
                                effectiveStartTime, startTime);
                } else {
                    Logger::warning("No keyframe found before startTime, seeking to original position");
                    ret = av_seek_frame(inputCtx, -1, static_cast<int64_t>(startTime * AV_TIME_BASE), AVSEEK_FLAG_BACKWARD);
                }
            }
        }
        
        // Copy packets from keyframe to end
        {
            AVPacket* packet = av_packet_alloc();
            if (!packet) {
                Logger::error("Could not allocate packet");
                goto cleanup;
            }
            
            std::vector<int64_t> firstPtsPerStream(inputCtx->nb_streams, AV_NOPTS_VALUE);
            
            while (av_read_frame(inputCtx, packet) >= 0) {
                AVStream* inputStream = inputCtx->streams[packet->stream_index];
                AVStream* outputStream = outputCtx->streams[packet->stream_index];
                
                // Convert packet timestamp to seconds for comparison
                double packetTime = 0.0;
                if (packet->pts != AV_NOPTS_VALUE) {
                    packetTime = static_cast<double>(packet->pts) * av_q2d(inputStream->time_base);
                } else if (packet->dts != AV_NOPTS_VALUE) {
                    packetTime = static_cast<double>(packet->dts) * av_q2d(inputStream->time_base);
                }
                
                // Skip packets before the effective start time (all streams use same start)
                if (packetTime < effectiveStartTime) {
                    av_packet_unref(packet);
                    continue;
                }
                
                // Record the first packet timestamp for offset calculation (per stream)
                int streamIndex = packet->stream_index;
                int64_t& streamFirstPts = firstPtsPerStream[streamIndex];
                if (streamFirstPts == AV_NOPTS_VALUE) {
                    if (packet->pts != AV_NOPTS_VALUE) {
                        streamFirstPts = av_rescale_q(packet->pts, inputStream->time_base, outputStream->time_base);
                    } else if (packet->dts != AV_NOPTS_VALUE) {
                        streamFirstPts = av_rescale_q(packet->dts, inputStream->time_base, outputStream->time_base);
                    }

                    if (streamFirstPts != AV_NOPTS_VALUE) {
                        double offsetSeconds = static_cast<double>(streamFirstPts) *
                                               av_q2d(outputStream->time_base);
                        Logger::info("Stream %d offset initialized to %.3f seconds", streamIndex, offsetSeconds);
                    }
                }
                
                // Rescale timestamps
                if (packet->pts != AV_NOPTS_VALUE) {
                    packet->pts = av_rescale_q(packet->pts, inputStream->time_base, outputStream->time_base);
                    if (streamFirstPts != AV_NOPTS_VALUE) {
                        packet->pts -= streamFirstPts;
                    }
                }
                
                if (packet->dts != AV_NOPTS_VALUE) {
                    packet->dts = av_rescale_q(packet->dts, inputStream->time_base, outputStream->time_base);
                    if (streamFirstPts != AV_NOPTS_VALUE) {
                        packet->dts -= streamFirstPts;
                    }
                }
                
                if (packet->duration > 0) {
                    packet->duration = av_rescale_q(packet->duration, inputStream->time_base, outputStream->time_base);
                }
                
                packet->pos = -1;
                
                // Write packet
                ret = av_interleaved_write_frame(outputCtx, packet);
                if (ret < 0) {
                    Logger::error("Error writing packet: %s", av_error_string(ret).c_str());
                    av_packet_free(&packet);
                    goto cleanup;
                }
                
                av_packet_unref(packet);
            }
            
            av_packet_free(&packet);
        }
        
        // Write trailer
        ret = av_write_trailer(outputCtx);
        if (ret < 0) {
            Logger::error("Error writing trailer: %s", av_error_string(ret).c_str());
            goto cleanup;
        }
        
        // Cleanup
        avformat_close_input(&inputCtx);
        if (outputCtx && !(outputCtx->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&outputCtx->pb);
        }
        avformat_free_context(outputCtx);
        
        Logger::info("Successfully trimmed video to last %d seconds using libavformat", durationSeconds);
        return true;
        
    cleanup:
        if (inputCtx) {
            avformat_close_input(&inputCtx);
        }
        if (outputCtx) {
            if (outputCtx->pb && !(outputCtx->oformat->flags & AVFMT_NOFILE)) {
                avio_closep(&outputCtx->pb);
            }
            avformat_free_context(outputCtx);
        }
        return false;
        
    } catch (const std::exception& e) {
        Logger::error("Exception in video trimming: %s", e.what());
        return false;
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
    // If context is provided, try to extract duration from it first
    // (though this is unlikely to help since we already tried this inline)
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


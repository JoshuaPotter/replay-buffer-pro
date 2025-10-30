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

namespace ReplayBufferPro {

bool VideoTrimmer::trimToLastSeconds(const std::string& inputPath,
                                   const std::string& outputPath,
                                   int durationSeconds) {
    initializeFFmpeg();
    
    AVFormatContext* inputCtx = nullptr;
    AVFormatContext* outputCtx = nullptr;
    
    // Declare variables before any goto statements to avoid C2362 errors
    int64_t seekTarget = 0;
    bool success = false;
    int videoStreamIndex = -1;
    int64_t keyframeTime = AV_NOPTS_VALUE;
    
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
        
        // Get total duration
        double totalDuration = getVideoDuration(inputPath);
        if (totalDuration <= 0) {
            Logger::error("Could not determine video duration or file is empty");
            avformat_close_input(&inputCtx);
            return false;
        }
        
        Logger::info("Input video duration: %.2f seconds", totalDuration);
        
        // Calculate start time (total duration - desired duration)
        // Ensure we don't go before the beginning of the file
        double startTime = std::max(0.0, totalDuration - durationSeconds);
        double actualDuration = totalDuration - startTime;
        
        Logger::info("Trimming from %.2f seconds to end (%.2f seconds total)", 
                    startTime, actualDuration);
        
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
        
        // Find the first keyframe at or after our desired start time
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
                        
                        // Check if this is a keyframe at or after our start time
                        if (packetTime >= startTime && (searchPacket->flags & AV_PKT_FLAG_KEY)) {
                            keyframeTime = searchPacket->pts;
                            Logger::info("Found keyframe at %.2f seconds (requested %.2f)", 
                                       packetTime, startTime);
                            break;
                        }
                    }
                    av_packet_unref(searchPacket);
                }
                av_packet_free(&searchPacket);
                
                // Seek back to the keyframe
                if (keyframeTime != AV_NOPTS_VALUE) {
                    int64_t keyframeSeekTarget = av_rescale_q(keyframeTime, 
                        inputCtx->streams[videoStreamIndex]->time_base, AV_TIME_BASE_Q);
                    ret = av_seek_frame(inputCtx, -1, keyframeSeekTarget, AVSEEK_FLAG_BACKWARD);
                    if (ret < 0) {
                        Logger::error("Error seeking to keyframe: %s", av_error_string(ret).c_str());
                    }
                } else {
                    Logger::warning("No keyframe found, seeking back to original position");
                    ret = av_seek_frame(inputCtx, -1, seekTarget, AVSEEK_FLAG_BACKWARD);
                }
            }
        }
        
        // Copy packets from keyframe to end
        success = false;
        {
            AVPacket* packet = av_packet_alloc();
            if (!packet) {
                Logger::error("Could not allocate packet");
                goto cleanup;
            }
            
            int64_t firstPts = AV_NOPTS_VALUE;
            bool foundFirstPacket = false;
            
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
                
                // For video streams, start from the keyframe we found
                // For audio streams, use the original start time
                double effectiveStartTime = startTime;
                if (packet->stream_index == videoStreamIndex && keyframeTime != AV_NOPTS_VALUE) {
                    effectiveStartTime = static_cast<double>(keyframeTime) * 
                                       av_q2d(inputCtx->streams[videoStreamIndex]->time_base);
                }
                
                // Skip packets before our effective start time
                if (packetTime < effectiveStartTime) {
                    av_packet_unref(packet);
                    continue;
                }
                
                // Record the first packet timestamp for offset calculation
                if (!foundFirstPacket && packet->pts != AV_NOPTS_VALUE) {
                    firstPts = av_rescale_q(packet->pts, inputStream->time_base, outputStream->time_base);
                    foundFirstPacket = true;
                }
                
                // Rescale timestamps
                if (packet->pts != AV_NOPTS_VALUE) {
                    packet->pts = av_rescale_q(packet->pts, inputStream->time_base, outputStream->time_base);
                    if (foundFirstPacket) {
                        packet->pts -= firstPts;
                    }
                }
                
                if (packet->dts != AV_NOPTS_VALUE) {
                    packet->dts = av_rescale_q(packet->dts, inputStream->time_base, outputStream->time_base);
                    if (foundFirstPacket) {
                        packet->dts -= firstPts;
                    }
                }
                
                if (packet->duration > 0) {
                    packet->duration = av_rescale_q(packet->duration, inputStream->time_base, outputStream->time_base);
                }
                
                packet->pos = -1;
                packet->stream_index = packet->stream_index;
                
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
            success = true;
        }
        
        if (!success) {
            goto cleanup;
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

double VideoTrimmer::getVideoDuration(const std::string& inputPath) {
    AVFormatContext* ctx = nullptr;
    
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
    
    avformat_close_input(&ctx);
    return duration;
}

bool VideoTrimmer::copyStreamsWithTimeRange(AVFormatContext* inputCtx,
                                           AVFormatContext* outputCtx,
                                           double startTime,
                                           double endTime) {
    // This method is kept for potential future use
    // Currently the main logic is in trimToLastSeconds for simplicity
    return true;
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
        
        Logger::info("Setup output stream %d: codec=%s, time_base=%d/%d", 
                    i, avcodec_get_name(outputStream->codecpar->codec_id),
                    outputStream->time_base.num, outputStream->time_base.den);
    }
    
    return true;
}

} // namespace ReplayBufferPro


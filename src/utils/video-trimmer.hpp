/**
 * @file video-trimmer.hpp
 * @brief Video trimming utility using libavformat
 * @author Joshua Potter
 * @copyright GPL v2 or later
 *
 * This file provides video trimming functionality using FFmpeg's libavformat
 * library.
 */

#pragma once

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/timestamp.h>
}

#include <string>

namespace ReplayBufferPro {

/**
 * @brief Video trimming utility class using libavformat
 * 
 * This class provides static methods for trimming video files using FFmpeg's
 * libavformat library.
 */
class VideoTrimmer {
public:
    /**
     * @brief Trim video to last N seconds using libavformat
     * 
     * This method opens a video file, calculates the start time for the last
     * N seconds, and creates a new trimmed video file using stream copy
     * (no re-encoding) for maximum performance.
     * 
     * @param inputPath Input video file path
     * @param outputPath Output video file path  
     * @param durationSeconds Duration in seconds to keep from the end
     * @return true if successful, false otherwise
     */
    static bool trimToLastSeconds(const std::string& inputPath, 
                                 const std::string& outputPath,
                                 int durationSeconds);

private:
    /**
     * @brief Initialize FFmpeg libraries (call once)
     * 
     * Initializes the FFmpeg library system. This is called automatically
     * by trimToLastSeconds but can be called explicitly if needed.
     */
    static void initializeFFmpeg();
    
    /**
     * @brief Get duration of video file in seconds
     * 
     * Opens a video file and determines its total duration in seconds.
     * 
     * @param inputPath Path to the video file
     * @return Duration in seconds, or -1.0 if error
     */
    static double getVideoDuration(const std::string& inputPath);
    
    /**
     * @brief Copy streams from input to output with time range
     * 
     * Copies all streams from input to output format context within
     * the specified time range, using stream copy for performance.
     * 
     * @param inputCtx Input format context
     * @param outputCtx Output format context
     * @param startTime Start time in seconds
     * @param endTime End time in seconds (or -1 for end of file)
     * @return true if successful, false otherwise
     */
    static bool copyStreamsWithTimeRange(AVFormatContext* inputCtx,
                                        AVFormatContext* outputCtx,
                                        double startTime,
                                        double endTime = -1.0);
    
    /**
     * @brief Setup output streams to match input streams
     * 
     * Creates output streams that match the input streams, copying
     * codec parameters for stream copy operation.
     * 
     * @param inputCtx Input format context
     * @param outputCtx Output format context
     * @return true if successful, false otherwise
     */
    static bool setupOutputStreams(AVFormatContext* inputCtx,
                                  AVFormatContext* outputCtx);
};

} // namespace ReplayBufferPro


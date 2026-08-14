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
 * @brief Outcome of a trim attempt, including the numbers needed to diagnose it
 *
 * On failure, reason is a short stable token suitable for grepping out of an
 * OBS log; detail carries the underlying libav error text when there is one.
 */
struct TrimResult {
    bool success = false;
    std::string reason;         ///< Short failure token, empty on success
    std::string detail;         ///< Human-readable failure detail, may be empty
    double sourceDuration = 0.0;///< Duration of the input in seconds
    double requestedStart = 0.0;///< Where the cut was asked for, in seconds
    double cutAt = 0.0;         ///< Where the keyframe actually put it, in seconds
    int64_t packetsWritten = 0; ///< Packets copied to the output
};

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
     * The cut lands on the first keyframe at or before the requested start,
     * so the output can be longer than requested by up to one GOP. It is
     * never shorter.
     *
     * @param inputPath Input video file path
     * @param outputPath Output video file path
     * @param durationSeconds Duration in seconds to keep from the end
     * @return Outcome of the attempt, including diagnostic timings
     */
    static TrimResult trimToLastSeconds(const std::string& inputPath,
                                        const std::string& outputPath,
                                        int durationSeconds);

    /**
     * @brief Get duration of video file in seconds
     *
     * Used both internally and by callers verifying that a freshly written
     * trim is the length it should be.
     *
     * @param inputPath Path to the video file
     * @param inputCtx Optional already-open format context (may be nullptr)
     * @return Duration in seconds, or -1.0 if error
     */
    static double getVideoDuration(const std::string& inputPath, AVFormatContext* inputCtx = nullptr);

private:
    /**
     * @brief Initialize FFmpeg libraries (call once)
     *
     * Initializes the FFmpeg library system. This is called automatically
     * by trimToLastSeconds but can be called explicitly if needed.
     */
    static void initializeFFmpeg();

    /**
     * @brief Open an input file, retrying while it is briefly locked
     *
     * OBS kicks off AutoRemux on the same file as it fires the saved event,
     * and antivirus or cloud-sync clients hold new files open too, so the
     * first open can lose a race that a retry moments later wins.
     *
     * @param inputPath Path to open
     * @param inputCtx Receives the opened context
     * @param lastError Receives the final libav error code on failure
     * @return true if the file was opened
     */
    static bool openInputWithRetry(const std::string& inputPath,
                                   AVFormatContext** inputCtx,
                                   int* lastError);

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


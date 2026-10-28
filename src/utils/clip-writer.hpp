/**
 * @file clip-writer.hpp
 * @brief Writes buffered encoder packets straight to a clip file
 * @author Joshua Potter
 * @copyright GPL v2 or later
 *
 * A port of what OBS's replay buffer does when it saves: the stream setup
 * from obs-ffmpeg-mux.c and ffmpeg-mux.c, and the timestamp rebasing from
 * replay_buffer_save(). It runs in-process with libavformat instead of
 * piping packets to the obs-ffmpeg-mux helper.
 */

#pragma once

extern "C" {
#include <libavutil/pixfmt.h>
}

// OBS includes
#include <obs.h>

#include <cstdint>
#include <string>
#include <vector>

namespace ReplayBufferPro {

/**
 * @brief Video stream parameters, as OBS passes them to obs-ffmpeg-mux
 */
struct ClipVideoInfo {
    std::string codec;              ///< FFmpeg codec name, from obs_encoder_get_codec
    std::vector<uint8_t> extradata; ///< Codec header from obs_encoder_get_extra_data
    int bitrate = 0;                ///< kbps
    int width = 0;
    int height = 0;
    int fpsNum = 0;
    int fpsDen = 0;
    AVColorPrimaries primaries = AVCOL_PRI_UNSPECIFIED;
    AVColorTransferCharacteristic transfer = AVCOL_TRC_UNSPECIFIED;
    AVColorSpace colorspace = AVCOL_SPC_UNSPECIFIED;
    AVColorRange range = AVCOL_RANGE_UNSPECIFIED;
    AVChromaLocation chromaLocation = AVCHROMA_LOC_UNSPECIFIED;
    uint32_t codecTag = 0;
};

/**
 * @brief One audio track's parameters
 */
struct ClipAudioInfo {
    std::string name;               ///< Track title, from obs_encoder_get_name
    std::vector<uint8_t> extradata;
    int bitrate = 0;                ///< kbps
    int sampleRate = 0;
    int frameSize = 0;
    int primingSamples = 0;
    int channels = 0;
};

/**
 * @brief Everything needed to write a clip besides its packets
 */
struct ClipStreams {
    ClipVideoInfo video;
    std::string audioCodec;           ///< Shared by every track, as in obs-ffmpeg-mux
    std::vector<ClipAudioInfo> audio; ///< Indexed by the packets' track_idx
    std::string muxerSettings;        ///< The output's muxer_settings, e.g. movflags
};

/**
 * @brief Outcome of a write
 */
struct ClipWriteResult {
    bool success = false;
    std::string reason;          ///< Short failure token, empty on success
    std::string detail;          ///< libav error text, may be empty
    int64_t packetsWritten = 0;
};

class ClipWriter {
public:
    /**
     * @brief Reads the stream parameters of the replay buffer output
     *
     * Only formats whose output has been checked against OBS's are accepted:
     * MKV, MP4 and MOV (fragmented included), H.264, HEVC or AV1 video, AAC or
     * Opus audio, SDR. Anything else is reported so the caller can save
     * through OBS instead.
     *
     * Main thread only: it reads encoder settings and extra data.
     *
     * @param output The replay buffer output
     * @param streams Receives the parameters
     * @param unsupported Receives a short reason when this returns false
     * @return true if a clip can be written from memory for this output
     */
    static bool describeStreams(obs_output_t *output, ClipStreams *streams, std::string *unsupported);

    /**
     * @brief Writes packets to a new file
     *
     * Packets must start on a video keyframe, in the order the output
     * received them. They are rebased so the clip starts at zero.
     *
     * @param path Destination; its extension picks the container
     * @param streams Stream parameters from describeStreams()
     * @param packets The clip's packets
     */
    static ClipWriteResult write(const std::string &path, const ClipStreams &streams,
                                 const std::vector<encoder_packet> &packets);

    /**
     * @brief Builds the path OBS would give a replay saved now
     *
     * Uses the output's directory, filename format and extension, creating
     * the directory if needed.
     *
     * @return The path, or empty if the output has no directory
     */
    static std::string generatePath(obs_output_t *output);
};

} // namespace ReplayBufferPro

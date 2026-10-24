/**
 * @file clip-writer.cpp
 * @brief Implementation of writing clips from buffered packets
 * @author Joshua Potter
 * @copyright GPL v2 or later
 *
 * Stream setup and timestamps follow OBS 32.2's plugins/obs-ffmpeg:
 * add_video_encoder_params() / add_audio_encoder_params() and
 * replay_buffer_save() in obs-ffmpeg-mux.c, and create_video_stream(),
 * create_audio_stream() and ffmpeg_mux_packet() in ffmpeg-mux/ffmpeg-mux.c.
 * Diff against those when the OBS dependency is updated.
 */

#include "clip-writer.hpp"
#include "logger.hpp"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/dict.h>
#include <libavutil/error.h>
#include <libavutil/mathematics.h>
#include <libavutil/mem.h>
#include <libavutil/pixdesc.h>
}

// OBS includes
#include <obs.hpp>
#include <util/platform.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>

namespace ReplayBufferPro {

namespace {

std::string avErrorString(int errnum) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(errnum, errbuf, AV_ERROR_MAX_STRING_SIZE);
    return std::string(errbuf);
}

ClipWriteResult failure(ClipWriteResult result, std::string reason, std::string detail = {}) {
    result.success = false;
    result.reason = std::move(reason);
    result.detail = std::move(detail);
    return result;
}

bool isOneOf(const std::string &value, std::initializer_list<const char *> allowed) {
    return std::any_of(allowed.begin(), allowed.end(),
                       [&value](const char *candidate) { return value == candidate; });
}

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::vector<uint8_t> encoderExtraData(obs_encoder_t *encoder) {
    uint8_t *data = nullptr;
    size_t size = 0;
    if (!obs_encoder_get_extra_data(encoder, &data, &size) || !data || size == 0) {
        return {};
    }
    return std::vector<uint8_t>(data, data + size);
}

int encoderBitrate(obs_encoder_t *encoder) {
    OBSDataAutoRelease settings = obs_encoder_get_settings(encoder);
    return static_cast<int>(obs_data_get_int(settings, "bitrate"));
}

// obs_to_ffmpeg_video_format() in obs-ffmpeg-formats.h
AVPixelFormat toFFmpegPixelFormat(video_format format) {
    switch (format) {
    case VIDEO_FORMAT_I444: return AV_PIX_FMT_YUV444P;
    case VIDEO_FORMAT_I412: return AV_PIX_FMT_YUV444P12LE;
    case VIDEO_FORMAT_I420: return AV_PIX_FMT_YUV420P;
    case VIDEO_FORMAT_NV12: return AV_PIX_FMT_NV12;
    case VIDEO_FORMAT_YUY2: return AV_PIX_FMT_YUYV422;
    case VIDEO_FORMAT_UYVY: return AV_PIX_FMT_UYVY422;
    case VIDEO_FORMAT_YVYU: return AV_PIX_FMT_YVYU422;
    case VIDEO_FORMAT_RGBA: return AV_PIX_FMT_RGBA;
    case VIDEO_FORMAT_BGRA: return AV_PIX_FMT_BGRA;
    case VIDEO_FORMAT_BGRX: return AV_PIX_FMT_BGRA;
    case VIDEO_FORMAT_Y800: return AV_PIX_FMT_GRAY8;
    case VIDEO_FORMAT_BGR3: return AV_PIX_FMT_BGR24;
    case VIDEO_FORMAT_I422: return AV_PIX_FMT_YUV422P;
    case VIDEO_FORMAT_I210: return AV_PIX_FMT_YUV422P10LE;
    case VIDEO_FORMAT_I40A: return AV_PIX_FMT_YUVA420P;
    case VIDEO_FORMAT_I42A: return AV_PIX_FMT_YUVA422P;
    case VIDEO_FORMAT_YUVA: return AV_PIX_FMT_YUVA444P;
    case VIDEO_FORMAT_YA2L: return AV_PIX_FMT_YUVA444P12LE;
    case VIDEO_FORMAT_I010: return AV_PIX_FMT_YUV420P10LE;
    case VIDEO_FORMAT_P010: return AV_PIX_FMT_P010LE;
    case VIDEO_FORMAT_P216: return AV_PIX_FMT_P216LE;
    case VIDEO_FORMAT_P416: return AV_PIX_FMT_P416LE;
    default: return AV_PIX_FMT_NONE;
    }
}

// determine_chroma_location() in obs-ffmpeg-formats.h
AVChromaLocation chromaLocation(AVPixelFormat format, AVColorSpace colorspace) {
    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(format);
    if (desc) {
        if (desc->log2_chroma_h == 0) {
            if (desc->log2_chroma_w == 0) {
                return AVCHROMA_LOC_CENTER; // 4:4:4
            }
            if (desc->log2_chroma_w == 1) {
                return AVCHROMA_LOC_LEFT; // 4:2:2
            }
        } else if (desc->log2_chroma_h == 1 && desc->log2_chroma_w == 1) {
            // 4:2:0
            return (colorspace == AVCOL_SPC_BT2020_NCL) ? AVCHROMA_LOC_TOPLEFT : AVCHROMA_LOC_LEFT;
        }
    }
    return AVCHROMA_LOC_UNSPECIFIED;
}

// OBS stores the tag big-endian and swaps it on little-endian hosts
uint32_t codecTagFromSettings(int64_t value) {
    uint32_t tag = static_cast<uint32_t>(value);
    const uint16_t probe = 1;
    if (*reinterpret_cast<const uint8_t *>(&probe) == 1) {
        tag = ((tag >> 24) & 0x000000FF) | ((tag << 8) & 0x00FF0000) |
              ((tag >> 8) & 0x0000FF00) | ((tag << 24) & 0xFF000000);
    }
    return tag;
}

uint8_t *copyExtraData(const std::vector<uint8_t> &data) {
    if (data.empty()) {
        return nullptr;
    }
    auto *copy = static_cast<uint8_t *>(av_mallocz(data.size() + AV_INPUT_BUFFER_PADDING_SIZE));
    if (copy) {
        std::memcpy(copy, data.data(), data.size());
    }
    return copy;
}

void closeOutput(AVFormatContext **ctx) {
    if (!*ctx) {
        return;
    }
    if ((*ctx)->pb && !((*ctx)->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&(*ctx)->pb);
    }
    avformat_free_context(*ctx);
    *ctx = nullptr;
}

// rescale_ts() in ffmpeg-mux.c
int64_t rescaleTimestamp(int64_t value, AVRational codecTimeBase, AVRational streamTimeBase) {
    return av_rescale_q_rnd(value / codecTimeBase.num, codecTimeBase, streamTimeBase,
                            static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
}

} // namespace

//=============================================================================
// STREAM DESCRIPTION
//=============================================================================

bool ClipWriter::describeStreams(obs_output_t *output, ClipStreams *streams, std::string *unsupported) {
    *streams = ClipStreams();

    OBSDataAutoRelease outputSettings = obs_output_get_settings(output);
    const std::string extension = lowercase(obs_data_get_string(outputSettings, "extension"));
    if (!isOneOf(extension, {"mkv", "mp4", "mov"})) {
        *unsupported = "container-" + (extension.empty() ? std::string("unknown") : extension);
        return false;
    }
    streams->muxerSettings = obs_data_get_string(outputSettings, "muxer_settings");

    obs_encoder_t *videoEncoder = obs_output_get_video_encoder(output);
    if (!videoEncoder) {
        *unsupported = "no-video-encoder";
        return false;
    }

    ClipVideoInfo &video = streams->video;
    video.codec = obs_encoder_get_codec(videoEncoder) ? obs_encoder_get_codec(videoEncoder) : "";
    if (!isOneOf(video.codec, {"h264", "hevc", "av1"})) {
        *unsupported = "video-codec-" + (video.codec.empty() ? std::string("unknown") : video.codec);
        return false;
    }

    const video_output_info *info = video_output_get_info(obs_get_video());
    if (!info) {
        *unsupported = "no-video-info";
        return false;
    }

    switch (info->colorspace) {
    case VIDEO_CS_601:
        video.primaries = AVCOL_PRI_SMPTE170M;
        video.transfer = AVCOL_TRC_SMPTE170M;
        video.colorspace = AVCOL_SPC_SMPTE170M;
        break;
    case VIDEO_CS_DEFAULT:
    case VIDEO_CS_709:
        video.primaries = AVCOL_PRI_BT709;
        video.transfer = AVCOL_TRC_BT709;
        video.colorspace = AVCOL_SPC_BT709;
        break;
    case VIDEO_CS_SRGB:
        video.primaries = AVCOL_PRI_BT709;
        video.transfer = AVCOL_TRC_IEC61966_2_1;
        video.colorspace = AVCOL_SPC_BT709;
        break;
    default:
        // HDR (PQ, HLG) also carries mastering metadata that is not ported yet
        *unsupported = "hdr";
        return false;
    }

    video.range = (info->range == VIDEO_RANGE_FULL) ? AVCOL_RANGE_JPEG : AVCOL_RANGE_MPEG;
    video.chromaLocation = chromaLocation(toFFmpegPixelFormat(info->format), video.colorspace);
    video.fpsNum = static_cast<int>(info->fps_num);
    video.fpsDen = static_cast<int>(info->fps_den);
    video.width = static_cast<int>(obs_output_get_width(output));
    video.height = static_cast<int>(obs_output_get_height(output));
    video.extradata = encoderExtraData(videoEncoder);

    {
        OBSDataAutoRelease encoderSettings = obs_encoder_get_settings(videoEncoder);
        video.bitrate = static_cast<int>(obs_data_get_int(encoderSettings, "bitrate"));
        video.codecTag = codecTagFromSettings(obs_data_get_int(encoderSettings, "codec_type"));
    }

    const int channels = static_cast<int>(audio_output_get_channels(obs_get_audio()));
    for (size_t i = 0; i < MAX_AUDIO_MIXES; i++) {
        obs_encoder_t *audioEncoder = obs_output_get_audio_encoder(output, i);
        if (!audioEncoder) {
            break;
        }

        // obs-ffmpeg-mux names one audio codec for every track
        if (i == 0) {
            const char *codec = obs_encoder_get_codec(audioEncoder);
            streams->audioCodec = codec ? codec : "";
            if (!isOneOf(streams->audioCodec, {"aac", "opus"})) {
                *unsupported = "audio-codec-" +
                               (streams->audioCodec.empty() ? std::string("unknown") : streams->audioCodec);
                return false;
            }
        }

        ClipAudioInfo track;
        const char *name = obs_encoder_get_name(audioEncoder);
        track.name = name ? name : "";
        track.extradata = encoderExtraData(audioEncoder);
        track.bitrate = encoderBitrate(audioEncoder);
        track.sampleRate = static_cast<int>(obs_encoder_get_sample_rate(audioEncoder));
        track.frameSize = static_cast<int>(obs_encoder_get_frame_size(audioEncoder));
        track.primingSamples = static_cast<int>(obs_encoder_get_priming_samples(audioEncoder));
        track.channels = channels;
        streams->audio.push_back(std::move(track));
    }

    return true;
}

//=============================================================================
// WRITING
//=============================================================================

ClipWriteResult ClipWriter::write(const std::string &path, const ClipStreams &streams,
                                  const std::vector<encoder_packet> &packets) {
    ClipWriteResult result;

    // Rebase like replay_buffer_save(), with one difference. OBS rebases each
    // audio track from its own first packet, which is safe for OBS because its
    // buffer starts where the output aligned audio to video. A clip cut from
    // the middle has no such alignment, so here audio is rebased against the
    // first video packet's DTS on the shared microsecond clock, and audio from
    // before that point is dropped. DTS, not PTS: the output starts audio and
    // video at the same DTS, and video PTS runs ahead of it by the encoder's
    // reorder delay. Packets are then ordered by dts_usec from that origin.
    struct Ordered {
        const encoder_packet *packet;
        int64_t dtsUsec;
        int64_t pts;
        int64_t dts;
    };

    bool foundVideo = false;
    int64_t videoPtsOffset = 0;
    int64_t videoStartUsec = 0;
    std::array<bool, MAX_AUDIO_MIXES> foundAudio{};
    std::array<int64_t, MAX_AUDIO_MIXES> audioDtsOffset{};

    std::vector<Ordered> ordered;
    ordered.reserve(packets.size());

    for (const encoder_packet &packet : packets) {
        Ordered entry{&packet, packet.dts_usec, packet.pts, packet.dts};

        if (packet.type == OBS_ENCODER_VIDEO) {
            if (!foundVideo) {
                videoPtsOffset = packet.pts;
                videoStartUsec = packet.dts_usec;
                foundVideo = true;
            }
            entry.dtsUsec -= videoStartUsec;
            entry.dts -= videoPtsOffset;
            entry.pts -= videoPtsOffset;
        } else {
            // The ring starts each clip on a video keyframe, so the video
            // origin is known before any audio
            if (!foundVideo || packet.dts_usec < videoStartUsec ||
                packet.track_idx >= MAX_AUDIO_MIXES || packet.track_idx >= streams.audio.size()) {
                continue;
            }
            const size_t track = packet.track_idx;
            if (!foundAudio[track]) {
                foundAudio[track] = true;
                const int64_t startSamples =
                    av_rescale(packet.dts_usec - videoStartUsec, streams.audio[track].sampleRate, 1000000LL);
                audioDtsOffset[track] = packet.dts - startSamples;
            }
            entry.dtsUsec -= videoStartUsec;
            entry.dts -= audioDtsOffset[track];
            entry.pts -= audioDtsOffset[track];
        }

        ordered.push_back(entry);
    }

    if (!foundVideo) {
        return failure(result, "no-video-packets");
    }

    std::stable_sort(ordered.begin(), ordered.end(),
                     [](const Ordered &a, const Ordered &b) { return a.dtsUsec < b.dtsUsec; });

    AVFormatContext *outputCtx = nullptr;
    int ret = avformat_alloc_output_context2(&outputCtx, nullptr, nullptr, path.c_str());
    if (ret < 0 || !outputCtx) {
        return failure(result, "output-context-failed", avErrorString(ret));
    }

    // Video stream (create_video_stream)
    const AVCodecDescriptor *videoDesc = avcodec_descriptor_get_by_name(streams.video.codec.c_str());
    AVStream *videoStream = videoDesc ? avformat_new_stream(outputCtx, nullptr) : nullptr;
    if (!videoStream) {
        closeOutput(&outputCtx);
        return failure(result, "video-stream-failed", streams.video.codec);
    }

    const AVRational videoCodecTimeBase{streams.video.fpsDen, streams.video.fpsNum};
    {
        AVCodecParameters *par = videoStream->codecpar;
        par->codec_type = videoDesc->type;
        par->codec_id = videoDesc->id;
        par->codec_tag = streams.video.codecTag;
        par->bit_rate = static_cast<int64_t>(streams.video.bitrate) * 1000;
        par->width = streams.video.width;
        par->height = streams.video.height;
        par->color_primaries = streams.video.primaries;
        par->color_trc = streams.video.transfer;
        par->color_space = streams.video.colorspace;
        par->color_range = streams.video.range;
        par->chroma_location = streams.video.chromaLocation;
        par->extradata = copyExtraData(streams.video.extradata);
        par->extradata_size = par->extradata ? static_cast<int>(streams.video.extradata.size()) : 0;
        videoStream->time_base = videoCodecTimeBase;
        videoStream->avg_frame_rate = av_inv_q(videoCodecTimeBase);
    }

    // Audio streams (create_audio_stream)
    const AVCodecDescriptor *audioDesc = streams.audio.empty()
                                             ? nullptr
                                             : avcodec_descriptor_get_by_name(streams.audioCodec.c_str());
    std::vector<AVStream *> audioStreams;
    for (const ClipAudioInfo &track : streams.audio) {
        AVStream *stream = audioDesc ? avformat_new_stream(outputCtx, nullptr) : nullptr;
        if (!stream) {
            closeOutput(&outputCtx);
            return failure(result, "audio-stream-failed", streams.audioCodec);
        }

        av_dict_set(&stream->metadata, "title", track.name.c_str(), 0);
        stream->time_base = AVRational{1, track.sampleRate};

        AVCodecParameters *par = stream->codecpar;
        par->codec_type = audioDesc->type;
        par->codec_id = audioDesc->id;
        if (!(audioDesc->props & AV_CODEC_PROP_LOSSLESS)) {
            par->bit_rate = static_cast<int64_t>(track.bitrate) * 1000;
        }
        par->sample_rate = track.sampleRate;
        par->frame_size = track.frameSize;
        par->initial_padding = track.primingSamples;
        av_channel_layout_default(&par->ch_layout, track.channels);
        // libavutil's default for 5 channels is 5.0; OBS uses 4.1
        if (track.channels == 5) {
            av_channel_layout_uninit(&par->ch_layout);
            av_channel_layout_from_mask(&par->ch_layout, AV_CH_LAYOUT_4POINT1);
        }
        par->extradata = copyExtraData(track.extradata);
        par->extradata_size = par->extradata ? static_cast<int>(track.extradata.size()) : 0;

        audioStreams.push_back(stream);
    }

    if (!(outputCtx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&outputCtx->pb, path.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            closeOutput(&outputCtx);
            return failure(result, "open-output-failed", avErrorString(ret));
        }
    }

    AVDictionary *options = nullptr;
    if (!streams.muxerSettings.empty() &&
        av_dict_parse_string(&options, streams.muxerSettings.c_str(), "=", " ", 0) < 0) {
        Logger::warning("Could not parse muxer settings '%s'; writing without them",
                        streams.muxerSettings.c_str());
        av_dict_free(&options);
    }

    ret = avformat_write_header(outputCtx, &options);
    av_dict_free(&options);
    if (ret < 0) {
        closeOutput(&outputCtx);
        return failure(result, "write-header-failed", avErrorString(ret));
    }

    AVPacket *avPacket = av_packet_alloc();
    if (!avPacket) {
        closeOutput(&outputCtx);
        return failure(result, "packet-alloc-failed");
    }

    // ffmpeg_mux_packet()
    for (const Ordered &entry : ordered) {
        const encoder_packet &packet = *entry.packet;
        const bool isVideo = packet.type == OBS_ENCODER_VIDEO;
        AVStream *stream = isVideo ? videoStream : audioStreams[packet.track_idx];
        const AVRational codecTimeBase =
            isVideo ? videoCodecTimeBase : AVRational{1, streams.audio[packet.track_idx].sampleRate};

        avPacket->data = packet.data;
        avPacket->size = static_cast<int>(packet.size);
        avPacket->stream_index = stream->index;
        avPacket->pts = rescaleTimestamp(entry.pts, codecTimeBase, stream->time_base);
        avPacket->dts = rescaleTimestamp(entry.dts, codecTimeBase, stream->time_base);
        avPacket->flags = packet.keyframe ? AV_PKT_FLAG_KEY : 0;

        ret = av_interleaved_write_frame(outputCtx, avPacket);

        // ffmpeg-mux treats these two as non-fatal
        if (ret == AVERROR_INVALIDDATA || ret == AVERROR(EINVAL)) {
            continue;
        }
        if (ret < 0) {
            av_packet_free(&avPacket);
            av_write_trailer(outputCtx);
            closeOutput(&outputCtx);
            return failure(result, "write-packet-failed", avErrorString(ret));
        }
        result.packetsWritten++;
    }

    av_packet_free(&avPacket);

    ret = av_write_trailer(outputCtx);
    closeOutput(&outputCtx);
    if (ret < 0) {
        return failure(result, "write-trailer-failed", avErrorString(ret));
    }

    result.success = true;
    return result;
}

//=============================================================================
// FILE NAMES
//=============================================================================

std::string ClipWriter::generatePath(obs_output_t *output) {
    // generate_filename() in obs-ffmpeg-mux.c
    OBSDataAutoRelease settings = obs_output_get_settings(output);
    std::string directory = obs_data_get_string(settings, "directory");
    const char *format = obs_data_get_string(settings, "format");
    const char *extension = obs_data_get_string(settings, "extension");
    const bool space = obs_data_get_bool(settings, "allow_spaces");

    if (directory.empty()) {
        return std::string();
    }

    char *filename = os_generate_formatted_filename(extension, space, format);
    if (!filename) {
        return std::string();
    }

    std::replace(directory.begin(), directory.end(), '\\', '/');
    if (directory.back() != '/') {
        directory += '/';
    }

    std::string path = directory + filename;
    bfree(filename);

    // The format can contain directories of its own
    const size_t slash = path.find_last_of('/');
    if (slash != std::string::npos) {
        os_mkdirs(path.substr(0, slash).c_str());
    }

    return path;
}

} // namespace ReplayBufferPro

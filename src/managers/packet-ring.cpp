/**
 * @file packet-ring.cpp
 * @brief Implementation of the replay buffer packet mirror
 */

#include "managers/packet-ring.hpp"
#include "utils/packet-window.hpp"

// STL includes
#include <algorithm>
#include <utility>

namespace ReplayBufferPro
{
  //=============================================================================
  // PACKET REFS
  //=============================================================================

  PacketRefs::~PacketRefs()
  {
    release();
  }

  PacketRefs::PacketRefs(PacketRefs &&other) noexcept
      : packets(std::move(other.packets))
  {
    other.packets.clear();
  }

  PacketRefs &PacketRefs::operator=(PacketRefs &&other) noexcept
  {
    if (this != &other)
    {
      release();
      packets = std::move(other.packets);
      other.packets.clear();
    }
    return *this;
  }

  void PacketRefs::release()
  {
    for (auto &packet : packets)
    {
      obs_encoder_packet_release(&packet);
    }
    packets.clear();
  }

  //=============================================================================
  // CONSTRUCTORS & DESTRUCTOR
  //=============================================================================

  PacketRing::PacketRing(ReadyCallback onReady)
      : onReady(std::move(onReady))
  {
  }

  PacketRing::~PacketRing()
  {
    attach(nullptr);
  }

  //=============================================================================
  // ATTACHMENT
  //=============================================================================

  void PacketRing::attach(obs_output_t *newOutput)
  {
    // Removing takes the same mutex OBS holds while it runs the callbacks, so
    // once this returns no callback for the old output is in progress.
    if (output)
    {
      obs_output_remove_packet_callback(output, &PacketRing::onPacket, this);
    }

    {
      std::lock_guard<std::mutex> lock(mutex);
      clearLocked();
    }

    output = newOutput ? obs_output_get_ref(newOutput) : nullptr;

    if (output)
    {
      refreshLimits();
      obs_output_add_packet_callback(output, &PacketRing::onPacket, this);
    }
  }

  obs_output_t *PacketRing::attachedOutput() const
  {
    return output.Get();
  }

  void PacketRing::refreshLimits()
  {
    if (!output)
    {
      return;
    }

    OBSDataAutoRelease settings = obs_output_get_settings(output);

    std::lock_guard<std::mutex> lock(mutex);
    maxTimeUsec = obs_data_get_int(settings, "max_time_sec") * 1000000LL;
    maxSizeBytes = obs_data_get_int(settings, "max_size_mb") * (1024LL * 1024LL);
  }

  void PacketRing::clear()
  {
    std::lock_guard<std::mutex> lock(mutex);
    clearLocked();
  }

  void PacketRing::clearLocked()
  {
    for (auto &packet : packets)
    {
      obs_encoder_packet_release(&packet);
    }
    packets.clear();

    frontSeq = 0;
    curSize = 0;
    curTime = 0;
    keyframes = 0;
    waitingForKeyframe = true;
    filled = false;
    captures.clear();
  }

  bool PacketRing::covers(int durationSeconds) const
  {
    std::lock_guard<std::mutex> lock(mutex);
    return filled || spanSecondsLocked() >= static_cast<double>(durationSeconds);
  }

  double PacketRing::spanSecondsLocked() const
  {
    if (packets.empty())
    {
      return 0.0;
    }
    return static_cast<double>(packets.back().dts_usec - packets.front().dts_usec) / 1e6;
  }

  //=============================================================================
  // CAPTURES
  //=============================================================================

  uint64_t PacketRing::beginCapture(int64_t pressSysUsec)
  {
    std::lock_guard<std::mutex> lock(mutex);

    Capture capture;
    capture.id = nextCaptureId++;
    capture.pressSysUsec = pressSysUsec;
    captures.push_back(capture);
    return capture.id;
  }

  bool PacketRing::isCaptureReady(uint64_t captureId) const
  {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = std::find_if(captures.begin(), captures.end(),
                           [captureId](const Capture &c) { return c.id == captureId; });
    return it != captures.end() && it->ready;
  }

  void PacketRing::cancelCapture(uint64_t captureId)
  {
    std::lock_guard<std::mutex> lock(mutex);
    captures.erase(std::remove_if(captures.begin(), captures.end(),
                                  [captureId](const Capture &c) { return c.id == captureId; }),
                   captures.end());
  }

  CaptureResult PacketRing::takeCapture(uint64_t captureId, int durationSeconds)
  {
    CaptureResult result;
    std::lock_guard<std::mutex> lock(mutex);

    auto it = std::find_if(captures.begin(), captures.end(),
                           [captureId](const Capture &c) { return c.id == captureId; });
    if (it == captures.end())
    {
      result.reason = "capture-not-found";
      return result;
    }

    const Capture capture = *it;
    captures.erase(it);
    result.bufferedSeconds = spanSecondsLocked();

    if (!capture.ready)
    {
      result.reason = "capture-not-ready";
      return result;
    }

    // Purging pauses while a capture is open, so this only happens if the ring
    // was cleared in between
    if (capture.endSeq < frontSeq || capture.endSeq - frontSeq >= packets.size())
    {
      result.reason = "capture-purged";
      return result;
    }

    const size_t end = static_cast<size_t>(capture.endSeq - frontSeq) + 1;
    const auto start = findWindowStart(end, static_cast<int64_t>(durationSeconds) * 1000000LL,
                                       [this](size_t i) {
                                         const encoder_packet &p = packets[i];
                                         PacketTiming timing;
                                         timing.dtsUsec = p.dts_usec;
                                         timing.video = p.type == OBS_ENCODER_VIDEO;
                                         timing.keyframe = p.keyframe;
                                         return timing;
                                       });
    if (!start)
    {
      result.reason = "no-keyframe";
      return result;
    }

    result.packets.packets.reserve(end - *start);
    int64_t lastVideoDts = packets[*start].dts_usec;
    for (size_t i = *start; i < end; i++)
    {
      encoder_packet ref;
      obs_encoder_packet_ref(&ref, &packets[i]);
      result.packets.packets.push_back(ref);
      if (ref.type == OBS_ENCODER_VIDEO)
      {
        lastVideoDts = ref.dts_usec;
      }
    }

    result.clipSeconds = static_cast<double>(lastVideoDts - packets[*start].dts_usec) / 1e6;
    result.success = true;
    return result;
  }

  //=============================================================================
  // PACKET CALLBACK (interleave thread)
  //=============================================================================

  void PacketRing::onPacket(obs_output_t *, encoder_packet *packet, encoder_packet_time *, void *param)
  {
    static_cast<PacketRing *>(param)->push(packet);
  }

  void PacketRing::push(encoder_packet *packet)
  {
    // The replay buffer only writes its first video track
    if (!packet || (packet->type == OBS_ENCODER_VIDEO && packet->track_idx != 0))
    {
      return;
    }

    const bool keyframe = packet->type == OBS_ENCODER_VIDEO && packet->keyframe;
    std::vector<uint64_t> readyIds;

    {
      std::lock_guard<std::mutex> lock(mutex);

      // Attached mid-GOP: a clip can only start on a keyframe
      if (waitingForKeyframe && !keyframe)
      {
        return;
      }
      waitingForKeyframe = false;

      if (captures.empty())
      {
        purgeFor(*packet);
      }

      encoder_packet ref;
      obs_encoder_packet_ref(&ref, packet);

      if (packets.empty())
      {
        curTime = ref.dts_usec;
      }
      curSize += static_cast<int64_t>(ref.size);
      packets.push_back(ref);

      if (keyframe)
      {
        keyframes++;
      }

      // Same rule as OBS's save_ts: the save completes on the first packet
      // whose system time reaches the press, and includes it
      const uint64_t seq = frontSeq + packets.size() - 1;
      for (auto &capture : captures)
      {
        if (!capture.ready && ref.sys_dts_usec >= capture.pressSysUsec)
        {
          capture.ready = true;
          capture.endSeq = seq;
          readyIds.push_back(capture.id);
        }
      }
    }

    for (uint64_t id : readyIds)
    {
      onReady(id);
    }
  }

  // The purge mirrors replay_buffer_purge() in obs-ffmpeg-mux.c, so the ring
  // holds the same packets as OBS's own buffer.
  void PacketRing::purgeFor(const encoder_packet &incoming)
  {
    if (maxSizeBytes > 0)
    {
      if (packets.empty() || keyframes <= 2)
      {
        return;
      }

      while (!packets.empty() && (curSize + static_cast<int64_t>(incoming.size)) > maxSizeBytes)
      {
        purge();
      }
    }

    if (packets.empty() || keyframes <= 2 || maxTimeUsec <= 0)
    {
      return;
    }

    while (!packets.empty() && (incoming.dts_usec - curTime) > maxTimeUsec)
    {
      purge();
    }
  }

  void PacketRing::purge()
  {
    filled = true;

    if (purgeFront())
    {
      // Drop the rest of that GOP so the ring always starts on a keyframe
      while (!packets.empty())
      {
        const encoder_packet &front = packets.front();
        if (front.type == OBS_ENCODER_VIDEO && front.keyframe)
        {
          return;
        }
        purgeFront();
      }
    }
  }

  bool PacketRing::purgeFront()
  {
    if (packets.empty())
    {
      return false;
    }

    encoder_packet packet = packets.front();
    packets.pop_front();
    frontSeq++;

    const bool keyframe = packet.type == OBS_ENCODER_VIDEO && packet.keyframe;
    if (keyframe)
    {
      keyframes--;
    }

    if (packets.empty())
    {
      curSize = 0;
      curTime = 0;
    }
    else
    {
      curTime = packets.front().dts_usec;
      curSize -= static_cast<int64_t>(packet.size);
    }

    obs_encoder_packet_release(&packet);
    return keyframe;
  }

} // namespace ReplayBufferPro

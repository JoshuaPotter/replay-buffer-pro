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

  void PacketRing::dropPacketsLocked()
  {
    for (auto &packet : packets)
    {
      obs_encoder_packet_release(&packet);
    }
    frontSeq += packets.size();
    packets.clear();

    curSize = 0;
    keyframes = 0;
    waitingForKeyframe = true;
    filled = false;
  }

  void PacketRing::clearLocked()
  {
    dropPacketsLocked();
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

    // Packets referenced per hold of the lock
    constexpr size_t CHUNK = 2048;

    auto closeCapture = [this, captureId]() {
      captures.erase(std::remove_if(captures.begin(), captures.end(),
                                    [captureId](const Capture &c) { return c.id == captureId; }),
                     captures.end());
    };

    uint64_t startSeq = 0;
    uint64_t endSeq = 0;

    {
      std::lock_guard<std::mutex> lock(mutex);

      auto it = std::find_if(captures.begin(), captures.end(),
                             [captureId](const Capture &c) { return c.id == captureId; });
      if (it == captures.end())
      {
        result.reason = "capture-not-found";
        return result;
      }

      const Capture capture = *it;
      result.bufferedSeconds = spanSecondsLocked();

      if (!capture.ready)
      {
        closeCapture();
        result.reason = "capture-not-ready";
        return result;
      }

      // Purging pauses while a capture is open, so this only happens if the
      // timeline restarted in between
      if (capture.endSeq < frontSeq || capture.endSeq - frontSeq >= packets.size())
      {
        closeCapture();
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
        closeCapture();
        result.reason = "no-keyframe";
        return result;
      }

      startSeq = frontSeq + *start;
      endSeq = capture.endSeq + 1;
    }

    // The capture is still open, so nothing is purged and frontSeq holds until
    // it closes. Only a timeline restart drops packets meanwhile, so each chunk
    // checks the slice is still there.
    result.packets.packets.reserve(static_cast<size_t>(endSeq - startSeq));
    for (uint64_t seq = startSeq; seq < endSeq;)
    {
      std::lock_guard<std::mutex> lock(mutex);
      if (startSeq < frontSeq || endSeq - frontSeq > packets.size())
      {
        closeCapture();
        result.packets = PacketRefs();
        result.reason = "capture-purged";
        return result;
      }

      const uint64_t chunkEnd = std::min<uint64_t>(endSeq, seq + CHUNK);
      for (; seq < chunkEnd; seq++)
      {
        encoder_packet ref;
        obs_encoder_packet_ref(&ref, &packets[static_cast<size_t>(seq - frontSeq)]);
        result.packets.packets.push_back(ref);
      }
    }

    {
      std::lock_guard<std::mutex> lock(mutex);
      closeCapture();
    }

    int64_t lastVideoDts = result.packets.packets.front().dts_usec;
    for (const encoder_packet &packet : result.packets.packets)
    {
      if (packet.type == OBS_ENCODER_VIDEO)
      {
        lastVideoDts = packet.dts_usec;
      }
    }

    result.clipSeconds =
        static_cast<double>(lastVideoDts - result.packets.packets.front().dts_usec) / 1e6;
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

      // The output's timeline starts over at 0 each time the buffer starts. A
      // packet far behind the newest means a restart whose stop event was
      // suppressed, so drop the old session's packets. Open captures stay:
      // their presses are judged against packets of the new session.
      if (!packets.empty() && packet->dts_usec + RESTART_THRESHOLD_USEC < packets.back().dts_usec)
      {
        dropPacketsLocked();
      }

      // Attached mid-GOP: a clip can only start on a keyframe
      if (waitingForKeyframe && !keyframe)
      {
        return;
      }
      waitingForKeyframe = false;

      if (captures.empty())
      {
        struct Buffer
        {
          PacketRing &ring;
          bool empty() const { return ring.packets.empty(); }
          int keyframes() const { return ring.keyframes; }
          int64_t bytes() const { return ring.curSize; }
          int64_t frontDtsUsec() const { return ring.packets.front().dts_usec; }
          bool frontIsKeyframe() const
          {
            return ring.packets.front().type == OBS_ENCODER_VIDEO && ring.packets.front().keyframe;
          }
          bool popFront()
          {
            ring.filled = true;
            return ring.purgeFront();
          }
        } buffer{*this};

        purgeBuffer(buffer, packet->dts_usec, static_cast<int64_t>(packet->size), maxTimeUsec,
                    maxSizeBytes);
      }

      encoder_packet ref;
      obs_encoder_packet_ref(&ref, packet);
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

    // As purge_front(): an emptied buffer resets its byte count outright
    curSize = packets.empty() ? 0 : curSize - static_cast<int64_t>(packet.size);

    obs_encoder_packet_release(&packet);
    return keyframe;
  }

} // namespace ReplayBufferPro

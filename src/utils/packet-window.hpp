/**
 * @file packet-window.hpp
 * @brief Chooses which buffered packets make up a clip, and which to purge
 * @author Joshua Potter
 * @copyright GPL v2 or later
 *
 * Pure logic with no OBS dependency, so it can be unit tested. The packet ring
 * calls it with an accessor over its own storage.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

namespace ReplayBufferPro
{
  /**
   * @brief The fields of a buffered packet that decide where a clip starts
   */
  struct PacketTiming
  {
    int64_t dtsUsec = 0; ///< Decode time on the stream clock, in microseconds
    bool video = false;
    bool keyframe = false;
  };

  /**
   * @brief Finds the first packet of a clip that ends at packet end - 1
   *
   * The clip starts on the newest video keyframe at or before its last video
   * packet minus the duration, so it is never shorter than asked when the
   * buffer holds enough. When the buffer holds less, it starts on the oldest
   * video keyframe instead.
   *
   * @param end One past the clip's last packet
   * @param durationUsec Requested clip length in microseconds
   * @param at Accessor returning the PacketTiming of packet i, for i < end
   * @return Index of the first packet, or nullopt with no video keyframe before end
   */
  template <typename At>
  std::optional<size_t> findWindowStart(size_t end, int64_t durationUsec, At at)
  {
    std::optional<int64_t> lastVideoDts;
    for (size_t i = end; i > 0; i--)
    {
      const PacketTiming packet = at(i - 1);
      if (packet.video)
      {
        lastVideoDts = packet.dtsUsec;
        break;
      }
    }

    if (!lastVideoDts)
    {
      return std::nullopt;
    }

    const int64_t target = *lastVideoDts - durationUsec;
    std::optional<size_t> oldestKeyframe;
    for (size_t i = end; i > 0; i--)
    {
      const PacketTiming packet = at(i - 1);
      if (!packet.video || !packet.keyframe)
      {
        continue;
      }

      oldestKeyframe = i - 1;
      if (packet.dtsUsec <= target)
      {
        return i - 1;
      }
    }

    return oldestKeyframe;
  }

  /**
   * @brief Drops packets from the front of a buffer the way OBS's replay buffer does
   *
   * A port of replay_buffer_purge() in obs-ffmpeg-mux.c, run before each
   * packet is stored, so a mirror holds exactly what OBS holds: whole GOPs are
   * dropped while storing the incoming packet would exceed the size limit or
   * span more than the time limit, but never while the buffer holds two
   * keyframes or fewer.
   *
   * Buffer provides empty(), keyframes(), bytes(), frontDtsUsec(),
   * frontIsKeyframe() and popFront(), which removes the front packet and
   * returns whether it was a video keyframe.
   *
   * @param incomingDtsUsec dts_usec of the packet about to be stored
   * @param incomingBytes Its size
   * @param maxTimeUsec max_time_sec in microseconds, 0 for none
   * @param maxBytes max_size_mb in bytes, 0 for none
   */
  template <typename Buffer>
  void purgeBuffer(Buffer &buffer, int64_t incomingDtsUsec, int64_t incomingBytes,
                   int64_t maxTimeUsec, int64_t maxBytes)
  {
    // purge(): drop the front packet and, if it was a keyframe, the rest of its GOP
    auto purgeGop = [&buffer]() {
      if (buffer.popFront())
      {
        while (!buffer.empty() && !buffer.frontIsKeyframe())
        {
          buffer.popFront();
        }
      }
    };

    if (maxBytes > 0)
    {
      if (buffer.empty() || buffer.keyframes() <= 2)
      {
        return;
      }

      while (!buffer.empty() && buffer.bytes() + incomingBytes > maxBytes)
      {
        purgeGop();
      }
    }

    if (buffer.empty() || buffer.keyframes() <= 2 || maxTimeUsec <= 0)
    {
      return;
    }

    while (!buffer.empty() && incomingDtsUsec - buffer.frontDtsUsec() > maxTimeUsec)
    {
      purgeGop();
    }
  }

} // namespace ReplayBufferPro

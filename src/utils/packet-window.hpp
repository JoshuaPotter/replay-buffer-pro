/**
 * @file packet-window.hpp
 * @brief Chooses which buffered packets make up a clip
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

} // namespace ReplayBufferPro

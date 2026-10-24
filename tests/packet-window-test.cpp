// Unit tests for findWindowStart. Built only with -DENABLE_TESTS=ON.

#include "utils/packet-window.hpp"

#include <cstdio>
#include <optional>
#include <vector>

namespace
{
  using ReplayBufferPro::PacketTiming;

  constexpr int64_t SECOND = 1000000;

  /**
   * @brief One video packet per second from 0 to lastSecond, a keyframe every
   * keyframeEvery seconds, with an audio packet half a second after each
   */
  std::vector<PacketTiming> makeBuffer(int lastSecond, int keyframeEvery)
  {
    std::vector<PacketTiming> packets;
    for (int s = 0; s <= lastSecond; s++)
    {
      packets.push_back({s * SECOND, true, s % keyframeEvery == 0});
      packets.push_back({s * SECOND + SECOND / 2, false, false});
    }
    return packets;
  }

  /**
   * @brief Index of the video packet at a given second in makeBuffer's layout
   */
  size_t videoAt(int second)
  {
    return static_cast<size_t>(second) * 2;
  }

  struct Case
  {
    const char *name;
    std::vector<PacketTiming> packets;
    size_t end;
    int64_t durationUsec;
    std::optional<size_t> expected;
  };
} // namespace

int main()
{
  const std::vector<PacketTiming> buffer = makeBuffer(20, 2);
  const std::vector<PacketTiming> audioOnly = {{0, false, false}, {SECOND, false, false}};

  const Case cases[] = {
      // Last video packet is at 20s, so 5s back is 15s; the keyframe before it is 14s
      {"rounds back to a keyframe", buffer, buffer.size(), 5 * SECOND, videoAt(14)},
      {"keyframe exactly at the target", buffer, buffer.size(), 6 * SECOND, videoAt(14)},
      {"zero duration starts on the newest keyframe", buffer, buffer.size(), 0, videoAt(20)},
      {"longer than the buffer starts on the oldest keyframe", buffer, buffer.size(), 100 * SECOND, videoAt(0)},
      // Ending after the 10s video packet: last video is 10s, target 7s, keyframe 6s
      {"respects an earlier end", buffer, videoAt(10) + 1, 3 * SECOND, videoAt(6)},
      // Ending on the audio packet after 10s still measures from the 10s video packet
      {"trailing audio does not move the end", buffer, videoAt(10) + 2, 3 * SECOND, videoAt(6)},
      {"no video packets", audioOnly, audioOnly.size(), SECOND, std::nullopt},
      {"empty window", buffer, 0, SECOND, std::nullopt},
  };

  int failures = 0;
  for (const Case &c : cases)
  {
    const std::optional<size_t> actual = ReplayBufferPro::findWindowStart(
        c.end, c.durationUsec, [&c](size_t i) { return c.packets[i]; });
    if (actual == c.expected)
      continue;
    ++failures;
    std::printf("FAIL %s: expected %s%zu, got %s%zu\n", c.name,
                c.expected ? "" : "nullopt/", c.expected.value_or(0),
                actual ? "" : "nullopt/", actual.value_or(0));
  }

  const size_t total = sizeof(cases) / sizeof(cases[0]);
  std::printf("%d of %zu cases passed\n", static_cast<int>(total) - failures, total);
  return failures == 0 ? 0 : 1;
}

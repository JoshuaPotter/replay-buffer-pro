// Unit tests for findWindowStart and purgeBuffer. Built only with -DENABLE_TESTS=ON.

#include "utils/packet-window.hpp"

#include <cstdio>
#include <deque>
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

  /**
   * @brief A buffer kept the way PacketRing keeps one, for purgeBuffer
   */
  struct FakeBuffer
  {
    struct Packet
    {
      int64_t dtsUsec;
      int64_t bytes;
      bool video;
      bool keyframe;
    };

    std::deque<Packet> packets;
    int keyframeCount = 0;
    int64_t byteCount = 0;
    int64_t maxTimeUsec = 0;
    int64_t maxBytes = 0;

    bool empty() const { return packets.empty(); }
    int keyframes() const { return keyframeCount; }
    int64_t bytes() const { return byteCount; }
    int64_t frontDtsUsec() const { return packets.front().dtsUsec; }
    bool frontIsKeyframe() const { return packets.front().video && packets.front().keyframe; }
    bool popFront()
    {
      const Packet front = packets.front();
      packets.pop_front();
      const bool wasKeyframe = front.video && front.keyframe;
      keyframeCount -= wasKeyframe ? 1 : 0;
      byteCount = packets.empty() ? 0 : byteCount - front.bytes;
      return wasKeyframe;
    }

    void push(Packet packet)
    {
      ReplayBufferPro::purgeBuffer(*this, packet.dtsUsec, packet.bytes, maxTimeUsec, maxBytes);
      packets.push_back(packet);
      keyframeCount += (packet.video && packet.keyframe) ? 1 : 0;
      byteCount += packet.bytes;
    }

    /**
     * @brief makeBuffer's layout, 100 bytes per packet, pushed through purgeBuffer
     */
    void fill(int lastSecond, int keyframeEvery)
    {
      for (int s = 0; s <= lastSecond; s++)
      {
        push({s * SECOND, 100, true, s % keyframeEvery == 0});
        push({s * SECOND + SECOND / 2, 100, false, false});
      }
    }
  };

  struct PurgeCase
  {
    const char *name;
    int lastSecond;
    int keyframeEvery;
    int64_t maxTimeUsec;
    int64_t maxBytes;
    int64_t expectedFrontDtsUsec;
  };

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

  // The front must also always be a video keyframe once purging has started
  const PurgeCase purgeCases[] = {
      // Pushing 30s with a 10s limit purges while (30.5 - front) > 10, whole
      // GOPs at a time, so the front lands on the keyframe at 22s
      {"time limit drops whole GOPs", 30, 2, 10 * SECOND, 0, 22 * SECOND},
      {"no limits keeps everything", 30, 2, 0, 0, 0},
      // Keyframes only at 0 and 30s: never more than two, so nothing is purged
      {"two keyframes or fewer are never purged", 30, 30, 10 * SECOND, 0, 0},
      // 62 packets of 100 bytes; a 2000-byte cap keeps under 20 packets
      {"size limit drops whole GOPs", 30, 2, 0, 2000, 22 * SECOND},
      // The size limit applies first, then the time limit trims further
      {"both limits", 30, 2, 5 * SECOND, 2000, 26 * SECOND},
  };

  for (const PurgeCase &c : purgeCases)
  {
    FakeBuffer buffer;
    buffer.maxTimeUsec = c.maxTimeUsec;
    buffer.maxBytes = c.maxBytes;
    buffer.fill(c.lastSecond, c.keyframeEvery);

    const bool frontOk = buffer.frontIsKeyframe() && buffer.frontDtsUsec() == c.expectedFrontDtsUsec;
    const bool bytesOk = c.maxBytes == 0 || buffer.bytes() <= c.maxBytes;
    if (frontOk && bytesOk)
      continue;
    ++failures;
    std::printf("FAIL %s: front %lld (keyframe %d), bytes %lld\n", c.name,
                static_cast<long long>(buffer.frontDtsUsec()), buffer.frontIsKeyframe() ? 1 : 0,
                static_cast<long long>(buffer.bytes()));
  }

  const size_t total = sizeof(cases) / sizeof(cases[0]) + sizeof(purgeCases) / sizeof(purgeCases[0]);
  std::printf("%d of %zu cases passed\n", static_cast<int>(total) - failures, total);
  return failures == 0 ? 0 : 1;
}

/**
 * @file packet-ring.hpp
 * @brief Mirrors the replay buffer's encoded packets so clips can be written from memory
 * @author Joshua Potter
 * @copyright GPL v2 or later
 */

#pragma once

// OBS includes
#include <obs.hpp>

// STL includes
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace ReplayBufferPro
{
  /**
   * @brief Owns one reference to each packet in a list, released on destruction
   */
  class PacketRefs
  {
  public:
    PacketRefs() = default;
    ~PacketRefs();
    PacketRefs(PacketRefs &&other) noexcept;
    PacketRefs &operator=(PacketRefs &&other) noexcept;
    PacketRefs(const PacketRefs &) = delete;
    PacketRefs &operator=(const PacketRefs &) = delete;

    std::vector<encoder_packet> packets;

  private:
    void release();
  };

  /**
   * @brief Outcome of taking a finished capture out of the ring
   */
  struct CaptureResult
  {
    bool success = false;
    std::string reason;             ///< Short failure token, empty on success
    PacketRefs packets;             ///< The clip, oldest first
    double clipSeconds = 0.0;       ///< Video span of the clip
    double bufferedSeconds = 0.0;   ///< Video span of the whole ring at the time
  };

  /**
   * @brief A mirror of the replay buffer output's packet buffer
   *
   * OBS keeps the replay buffer in a private deque inside obs-ffmpeg-mux.c and
   * only ever writes all of it. obs_output_add_packet_callback() shows this
   * class every packet the output receives, just before the output stores it.
   * Each one is held by reference, which shares OBS's own copy of the data, and
   * purged by the same rules as replay_buffer_purge(), so the ring holds the
   * same packets as OBS at almost no extra memory.
   *
   * A capture marks the end of a clip the way OBS's save_ts does: the first
   * packet whose system time reaches the press. Purging pauses while any
   * capture is open, so its start cannot be dropped before it is taken.
   *
   * attach(), clear() and the capture calls run on the Qt main thread. The
   * packet callback runs on OBS's interleave thread, which an encoder shares
   * with every output it feeds, so it only takes a short lock and never blocks.
   */
  class PacketRing
  {
  public:
    /**
     * @brief Called from the interleave thread when a capture's end packet arrives
     *
     * Must not block; post to the main thread and return.
     */
    using ReadyCallback = std::function<void(uint64_t captureId)>;

    explicit PacketRing(ReadyCallback onReady);
    ~PacketRing();

    /**
     * @brief Starts mirroring an output, or stops with nullptr
     *
     * Drops everything held for the previous output, including open captures.
     * Once this returns no callback for the previous output is running.
     */
    void attach(obs_output_t *output);

    /**
     * @brief The output being mirrored, or nullptr
     */
    obs_output_t *attachedOutput() const;

    /**
     * @brief Reads max_time_sec and max_size_mb from the output's settings
     *
     * OBS reads them only when the buffer starts, so call this on attach and
     * on every REPLAY_BUFFER_STARTED.
     */
    void refreshLimits();

    /**
     * @brief Drops every packet and open capture
     *
     * Call when the buffer stops; OBS clears its own buffer then too.
     */
    void clear();

    /**
     * @brief Whether the ring holds the last durationSeconds of the buffer
     *
     * False until the ring has filled once when it holds less than asked, as
     * when the plugin attached after the buffer started.
     */
    bool covers(int durationSeconds) const;

    /**
     * @brief Opens a capture that ends at the first packet at or after the press
     * @param pressSysUsec Press time, os_gettime_ns() / 1000
     * @return Capture id passed to the ready callback and takeCapture()
     */
    uint64_t beginCapture(int64_t pressSysUsec);

    /**
     * @brief Takes a capture whose end packet has arrived
     *
     * Takes the lock in short spans while it references the clip's packets,
     * because the interleave thread waits on it for every packet. The capture
     * stays open meanwhile, so purging stays paused and indices stay put.
     *
     * @param captureId Id from beginCapture()
     * @param durationSeconds Requested clip length
     * @return The clip, or a failure reason; the capture is closed either way
     */
    CaptureResult takeCapture(uint64_t captureId, int durationSeconds);

    /**
     * @brief Whether a capture's end packet has arrived
     */
    bool isCaptureReady(uint64_t captureId) const;

    /**
     * @brief Closes a capture without taking it
     */
    void cancelCapture(uint64_t captureId);

  private:
    struct Capture
    {
      uint64_t id = 0;
      int64_t pressSysUsec = 0;
      bool ready = false;
      uint64_t endSeq = 0; ///< Sequence number of the end packet, once ready
    };

    static void onPacket(obs_output_t *output, encoder_packet *packet,
                         encoder_packet_time *packetTime, void *param);
    void push(encoder_packet *packet);
    bool purgeFront();
    void dropPacketsLocked();
    void clearLocked();
    double spanSecondsLocked() const;

    /// A packet this far behind the newest means the timeline started over
    static constexpr int64_t RESTART_THRESHOLD_USEC = 5 * 1000000LL;

    ReadyCallback onReady;

    // Main thread only
    OBSOutputAutoRelease output;

    // Guarded by mutex; shared with the interleave thread
    mutable std::mutex mutex;
    std::deque<encoder_packet> packets;
    uint64_t frontSeq = 0;         ///< Sequence number of packets.front()
    int64_t curSize = 0;           ///< Bytes held, as OBS's cur_size
    int keyframes = 0;             ///< Video keyframes held, as OBS's keyframes
    int64_t maxTimeUsec = 0;
    int64_t maxSizeBytes = 0;
    bool waitingForKeyframe = true; ///< Drop packets until a clip could start
    bool filled = false;           ///< Has purged at least once since it was cleared
    std::vector<Capture> captures;
    uint64_t nextCaptureId = 1;
  };

} // namespace ReplayBufferPro

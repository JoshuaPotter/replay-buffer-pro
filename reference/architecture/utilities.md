# Utilities and Shared Infrastructure

This document covers shared utilities used across the plugin.

## OBS data RAII
- `OBSDataRAII` wraps `obs_data_t` and releases it in the destructor.
- It is a simple ownership wrapper used by settings and hotkey persistence.
- Copying is disabled to avoid double-free; move semantics are not implemented.

## Logging
- `Logger` provides `info`, `warning`, and `error` helpers.
- Each helper writes to OBS logs using `blog(...)` with the `[ReplayBufferPro]` prefix.
- Buffer size is fixed to 4096 chars per log call.

## Duration formatting
- `duration-format` formats localized duration labels (seconds/minutes/hours).
- Used for save button labels and hotkey descriptions.

## Status bar messages
- `StatusReporter::showMessage(text, timeoutMs)` posts transient text to the OBS main window status bar.
- It reaches the bar through `obs_frontend_get_main_window()` and marshals onto the Qt main thread, so it is safe to call from the trim worker thread.
- It does nothing when the main window is unavailable, which is the case during shutdown.

## Video trimming (FFmpeg libavformat)
`VideoTrimmer` trims a saved replay file down to the last N seconds using stream copy (no re-encoding). It follows this sequence:
1. Open the input file, retrying with backoff while another process still holds it, and find stream info.
2. Determine total duration from container or stream durations.
3. Calculate start time: `max(0, totalDuration - durationSeconds)`.
4. Create output format context and mirror input streams.
5. Seek backwards to the start time and take the first key video packet as the cut point.
6. Copy packets from the cut point to the end.
7. Rescale timestamps per stream so output starts at 0.
8. Write trailer and close contexts.

It returns a `TrimResult` carrying the source duration, requested start, actual cut point and packet count, which the caller uses both to verify the output and to build its log verdict.

### Cut point selection
- `AVSEEK_FLAG_BACKWARD` already lands on the closest keyframe at or before the target, so the first key packet after the seek is the cut point. If seeking is unavailable, the scan falls back to tracking the last keyframe at or before the target.
- Timestamps are compared as **DTS**, which is monotonic. Comparing PTS lets a reordered B-frame end the search early and move the cut back by a whole GOP.
- Because the cut only ever lands at or before the request, output is never shorter than asked and can be up to one GOP longer. Drift past `Config::TRIM_KEYFRAME_TOLERANCE_SECONDS` is logged as a warning.

### Timestamp handling
- All streams are rebased by the same offset so they stay in sync.
- Filtering on DTS keeps the shifted timestamps non-negative, since `DTS <= PTS` holds for every valid packet.
- A packet missing only one timestamp is passed through; the muxer infers the rest. Substituting PTS for a missing DTS reorders B-frames badly enough to fail the write.
- A packet missing both timestamps is stepped on from the previous packet in that stream rather than dropped, which would otherwise punch a hole in the clip.

### Stream setup details
- `setupOutputStreams(...)` copies codec parameters and metadata.
- Stream time bases are preserved.
- `codec_tag` is cleared to avoid container mismatch issues.

### Error handling
- Failures return a `TrimResult` with `success == false` and a short stable `reason` token, plus the libav error text in `detail`.
- If any step fails, the input and output contexts are closed and the partial output is left for the caller to delete.
- Copying zero packets is treated as a failure (`no-packets-written`) rather than writing a valid but empty file, which would otherwise cost the caller the original.

## Related code
- `src/utils/obs-utils.hpp`
- `src/utils/obs-utils.cpp`
- `src/utils/duration-format.hpp`
- `src/utils/duration-format.cpp`
- `src/utils/logger.hpp`
- `src/utils/status-reporter.hpp`
- `src/utils/status-reporter.cpp`
- `src/utils/video-trimmer.hpp`
- `src/utils/video-trimmer.cpp`

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

## Video trimming (FFmpeg libavformat)
`VideoTrimmer` trims a saved replay file down to the last N seconds using stream copy (no re-encoding). It follows this sequence:
1. Open the input file and find stream info.
2. Determine total duration from container or stream durations.
3. Calculate start time: `max(0, totalDuration - durationSeconds)`.
4. Create output format context and mirror input streams.
5. Seek to the start time and locate a keyframe at or after the target.
6. Copy packets from the effective start time to the end.
7. Rescale timestamps per stream so output starts at 0.
8. Write trailer and close contexts.

### Stream setup details
- `setupOutputStreams(...)` copies codec parameters and metadata.
- Stream time bases are preserved.
- `codec_tag` is cleared to avoid container mismatch issues.

### Error handling
- Errors are logged and return `false` to the caller.
- If any step fails, the output context is closed and the call ends.

## Related code
- `src/utils/obs-utils.hpp`
- `src/utils/obs-utils.cpp`
- `src/utils/logger.hpp`
- `src/utils/video-trimmer.hpp`
- `src/utils/video-trimmer.cpp`

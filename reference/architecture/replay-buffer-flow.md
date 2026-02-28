# Replay Buffer Flow (Save + Trim)

This document explains how the plugin saves replay buffer content and trims it to a selected duration.

## Responsibilities
- Trigger replay buffer saves through OBS frontend APIs.
- Track the requested duration for the saved file.
- Trim the saved file to the last N seconds using FFmpeg (libavformat).

## Save segment flow
1. User clicks a duration button or hotkey.
2. `ReplayBufferManager::saveSegment(duration, parent)` validates:
   - Replay buffer is active.
   - `duration <= currentBufferLength` from `SettingsManager`.
3. If valid, `pendingSaveDuration` is set and `obs_frontend_replay_buffer_save()` is called.
4. OBS emits `OBS_FRONTEND_EVENT_REPLAY_BUFFER_SAVED`.
5. The dock calls `handleReplayBufferSaved()`:
   - Retrieves the saved path via `obs_frontend_get_last_replay()`.
   - Copies the path, frees the OBS-allocated buffer, and trims in a background thread.
   - Clears `pendingSaveDuration`.

## Save full buffer flow
1. User clicks “Save Replay Buffer”.
2. `ReplayBufferManager::saveFullBuffer(...)` checks buffer activity.
3. If active, it calls `obs_frontend_replay_buffer_save()` and does not set a pending duration.
4. When OBS signals a saved replay, no trim is performed because the pending duration is 0.

## Trimming details
- `ReplayBufferManager::trimReplayBuffer(...)`:
  - Builds output path by inserting `_trimmed` before the extension.
  - Calls `VideoTrimmer::trimToLastSeconds(...)`.
  - Deletes the original file with `os_unlink(...)` on success.

## Error handling
- UI warnings show when the replay buffer is inactive or the requested duration is too long.
- Trimming errors are logged via `Logger::error(...)` but do not raise UI alerts.
- If no saved replay path is returned, trimming is skipped.

## Key classes and functions
- `ReplayBufferManager::saveSegment(...)`
- `ReplayBufferManager::saveFullBuffer(...)`
- `ReplayBufferManager::getPendingSaveDuration()`
- `ReplayBufferManager::trimReplayBuffer(...)`
- `VideoTrimmer::trimToLastSeconds(...)`

## Related code
- `src/managers/replay-buffer-manager.hpp`
- `src/managers/replay-buffer-manager.cpp`
- `src/utils/video-trimmer.hpp`
- `src/utils/video-trimmer.cpp`

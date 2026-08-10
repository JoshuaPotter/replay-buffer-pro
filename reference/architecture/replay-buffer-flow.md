# Replay Buffer Flow (Save + Trim)

This document explains how the plugin saves replay buffer content and trims it to a selected duration.

## Responsibilities
- Trigger replay buffer saves through OBS frontend APIs.
- Track the requested duration for the saved file.
- Trim the saved file to the last N seconds using FFmpeg (libavformat).
- Report the outcome of every save, so an untrimmed clip is diagnosable after the fact.

## Correlating requests with saved files

OBS provides no way to tie a save request to the file it eventually produces. `obs_frontend_replay_buffer_save()` only arms a timestamp inside the replay buffer output; the mux starts on the next encoded packet and the `saved` event fires when the file is complete, which can be seconds later.

`ReplayBufferManager` therefore keeps a FIFO of pending requests and matches them to saved events in order:

- Each request records its duration and the time it was made. A duration of `0` is an explicit "do not trim" marker used by Save Replay Buffer.
- Requests expire after `Config::TRIM_REQUEST_TIMEOUT_MS`. OBS silently drops a save when the encoders are paused or the output is inactive, and without expiry that orphaned duration would later be applied to an unrelated clip.
- Requests made within `Config::TRIM_REQUEST_COALESCE_MS` of each other are collapsed into one, because OBS tracks a single pending save timestamp and produces only one file for presses that close together.
- A saved event with nothing pending came from outside the plugin (OBS's own Save Replay hotkey, the tray item, obs-websocket). It is logged and left alone.

## Save segment flow
1. User clicks a duration button or hotkey.
2. `ReplayBufferManager::saveSegment(duration, parent)` validates:
   - Replay buffer is active.
   - `duration <= currentBufferLength` from `SettingsManager`.
3. If valid, the request is queued and `obs_frontend_replay_buffer_save()` is called.
4. OBS emits `OBS_FRONTEND_EVENT_REPLAY_BUFFER_SAVED`.
5. `Plugin::handleReplayBufferSaved()` reads the path via `obs_frontend_get_last_replay()` and hands it to `ReplayBufferManager::handleSaveCompleted(...)`.
6. The manager pops the matching request and queues a trim job for its worker thread.

## Save full buffer flow
1. User clicks "Save Replay Buffer".
2. `ReplayBufferManager::saveFullBuffer(...)` checks buffer activity.
3. If active, it queues a `0` request and calls `obs_frontend_replay_buffer_save()`.
4. The saved event consumes that marker and skips trimming. Queueing the marker rather than nothing is what stops the save from inheriting a stale duration from an earlier request.

## Trimming details

Trims run on a single worker thread owned by `ReplayBufferManager`, joined in its destructor. One trim runs at a time, and none can outlive the manager.

`processTrimJob(...)` commits in stages so a failure never costs the user their clip:

1. Trim into `<name>.rbp-partial.<ext>`, never straight to the final name.
2. Verify the result with `VideoTrimmer::getVideoDuration(...)`. The output is rejected if it is unreadable, shorter than `Config::TRIM_MIN_DURATION_RATIO` of what was achievable, or longer than both `Config::TRIM_MAX_DURATION_RATIO` and `Config::TRIM_MAX_DURATION_SLACK_SECONDS` allow. The upper bound is deliberately loose because keyframe alignment legitimately makes clips longer, never shorter.
3. Rename the partial to `<name>_trimmed.<ext>`.
4. Delete the original, retrying while another process still holds it.

Any failure deletes the partial and leaves the original untouched.

### Cut point selection

`VideoTrimmer::trimToLastSeconds(...)` seeks backwards to the requested start, which lands on the closest keyframe at or before it, and takes the first key video packet from there as the cut point. Timestamps are compared as DTS, which is monotonic; comparing PTS lets a reordered B-frame end the search early and drag the cut back by a whole GOP.

The cut can only ever land at or before the request, so a clip is never shorter than asked and can be longer by up to one GOP. Drift beyond `Config::TRIM_KEYFRAME_TOLERANCE_SECONDS` is logged as a warning naming the encoder keyframe interval as the cause.

### Contention for the saved file

`OBSBasic::ReplayBufferSaved()` calls `AutoRemux(path)` immediately after firing the saved event, so when "Automatically remux to mp4" is enabled OBS is reading the same file the plugin is about to trim and delete. Antivirus and cloud-sync clients do the same to any newly written file. Both the input open and the original delete retry with backoff, and the AutoRemux setting is logged when a trim starts so the interaction is visible in the log.

## Diagnostics

Every `OBS_FRONTEND_EVENT_REPLAY_BUFFER_SAVED` produces exactly one verdict line, prefixed `TRIM VERDICT`:

```
[ReplayBufferPro] TRIM VERDICT: ok file='...' requested=30s actual=31.2s source=540.0s cut_at=508.8s elapsed=1.4s
[ReplayBufferPro] TRIM VERDICT: skipped reason=no-pending-request file='...'
[ReplayBufferPro] TRIM VERDICT: skipped reason=save-full-buffer file='...'
[ReplayBufferPro] TRIM VERDICT: failed reason=open-input-failed detail='...' file='...' requested=30s elapsed=2.1s original-kept
```

Grepping an OBS log for `TRIM VERDICT` gives a complete per-save accounting. The `reason` field names the cause:

| reason | meaning |
| --- | --- |
| `no-pending-request` | The save was triggered outside the plugin, so it was not trimmed |
| `save-full-buffer` | Save Replay Buffer, intentionally untrimmed |
| `no-saved-path` | OBS reported a save but no path |
| `open-input-failed` | The saved file could not be opened, even after retries |
| `output-too-long` | The cut point collapsed; the clip would have been near full length |
| `output-too-short` / `output-unreadable` | The written clip did not survive verification |
| `no-packets-written` | Nothing was copied; writing would have produced an empty clip |
| `rename-failed` | The verified clip could not take its final name |

Successes and failures also surface briefly in the OBS status bar via `StatusReporter`.

## Third-party compatibility
- [Smart Replay Mover](https://github.com/SlonickLab/Smart-Replay-Mover) (SlonickLab) is a third-party OBS Lua script that waits for this plugin to finish trimming, then moves the final `_trimmed` file into per-game folders. See [issue #23](https://github.com/JoshuaPotter/replay-buffer-pro/issues/23).
- The `_trimmed` suffix and the deletion of the original are unchanged. Trims are now written to a `.rbp-partial.<ext>` scratch file and renamed into place, so the `_trimmed` file appears atomically and is complete the moment it exists — previously a watcher could observe it partially written.
- If the `_trimmed` suffix, the original-file deletion, or the timing of when the final file appears changes, open an issue at https://github.com/SlonickLab/Smart-Replay-Mover so they can update their compatibility handling.

## Error handling
- UI warnings show when the replay buffer is inactive or the requested duration is too long.
- A failed trim leaves the original clip in place, logs a `failed` verdict naming the reason, and shows a status bar message.
- If no saved replay path is returned, trimming is skipped and logged.

## Key classes and functions
- `ReplayBufferManager::saveSegment(...)`
- `ReplayBufferManager::saveFullBuffer(...)`
- `ReplayBufferManager::handleSaveCompleted(...)`
- `ReplayBufferManager::processTrimJob(...)`
- `ReplayBufferManager::verifyTrimmedOutput(...)`
- `VideoTrimmer::trimToLastSeconds(...)`
- `StatusReporter::showMessage(...)`

## Related code
- `src/managers/replay-buffer-manager.hpp`
- `src/managers/replay-buffer-manager.cpp`
- `src/utils/video-trimmer.hpp`
- `src/utils/video-trimmer.cpp`
- `src/utils/status-reporter.hpp`
- `src/utils/status-reporter.cpp`

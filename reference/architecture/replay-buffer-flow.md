# Replay Buffer Flow (Save + Trim)

This document explains how the plugin saves replay buffer content and trims it to a selected duration.

## Responsibilities
- Trigger replay buffer saves through OBS frontend APIs.
- Track the requested duration for the saved file.
- Trim the saved file to the last N seconds using FFmpeg (libavformat).
- Report the outcome of every save, so an untrimmed clip is diagnosable after the fact.

## Correlating requests with saved files

OBS gives no way to tie a save request to the file it eventually produces, and it does not queue requests either. The facts the design is built on (OBS 32.2, `plugins/obs-ffmpeg/obs-ffmpeg-mux.c`):

- A save request is a single `int64_t`, `save_ts` (`replay_buffer_hotkey`, lines 917-942). A second request before OBS starts writing overwrites it, so any number of requests made in that window produce **one** file and **one** `saved` signal.
- A request that lands while a file is being written is held, not dropped: `replay_buffer_data` (lines 1236-1247) returns without clearing `save_ts` while `muxing` is set, then fires once the write finishes.
- OBS silently drops a save (no file, no signal) when the output is inactive or the video encoder is paused, e.g. because recording is paused.
- Stopping the buffer zeroes an armed `save_ts` (`replay_buffer_clear`, lines 47-62), but a write already in progress still completes and signals.
- The output's own `saved` signal (lines 1129-1134) fires on the mux thread after the mux subprocess has exited, so the file is complete and closed. It carries no parameters.
- `OBS_FRONTEND_EVENT_REPLAY_BUFFER_SAVED` is **not** reliable: `OBSBasic::ReplayBufferSaved()` returns early when the buffer has already stopped (`frontend/widgets/OBSBasic_ReplayBuffer.cpp:174`), and `OBSStudioAPI::on_event` drops every frontend event while `disableSaving` is set during scene collection and profile switches (`frontend/OBSStudioAPI.cpp:745-750`). In both cases the file exists but the event never arrives. `obs_frontend_get_last_replay()` is only updated after that early return, so after a stop it still names the previous clip.

An earlier design kept a FIFO of requests with a timeout. Because OBS collapses requests, the FIFO drifted: a request OBS never honored stayed at the head and was matched to the next unrelated file, applying the wrong duration; and a timeout short enough to limit that discarded requests whose files were merely slow to write (issue #40, a ~2m19s write).

`ReplayBufferManager` now mirrors OBS's own model. All correlation state lives on the Qt main thread.

- **At most one outstanding request** (issued to OBS, awaiting its file) and **one deferred request** (last-write-wins). Each file therefore pairs with exactly one request.
- **Pre-flight gate.** Before recording anything, the manager checks what OBS checks: the replay buffer output is active and its video encoder is not paused. A save OBS would drop is refused (`skipped reason=encoder-paused` / `buffer-inactive`) and leaves no state behind, so it can never claim a later file. This check is what retires the timeout.
- **Has OBS started a file?** Answered from two sources: the `get_last_replay` proc omits its `path` key while a mux is in flight (lines 944-949), and a counter bumped in the `saved` callback that the main thread has not yet caught up with means a file has finished but its completion is still queued.
- A press while nothing is outstanding and nothing has started is **armed**: `obs_frontend_replay_buffer_save()` is called.
- A press while our request is outstanding but OBS has not started its file is **folded** into it: the newest press sets the duration and no second save is issued, exactly as OBS folds it into `save_ts`. A double press or a Stream Deck double-fire gives one clip.
- A press while a file is being written is **deferred** and issued once that file completes. A newer press replaces an older deferred one.
- A press while nothing of ours is outstanding but a file has started is **waiting-foreign**: something else (OBS's own Save Replay hotkey, a Stream Deck OBS action, obs-websocket) is writing. A placeholder takes the outstanding slot so that file is reported `no-pending-request` rather than trimmed as ours, then our request is issued.
- **Completions come from the output's `saved` signal**, not the frontend event. The path is read with the `get_last_replay` proc inside the callback, on the mux thread, and posted to the main thread with the path attached.
- On `REPLAY_BUFFER_STOPPED` a deferred request is dropped, and the outstanding one is dropped only if OBS has not started its file. `STOPPING` does not clear anything, because packets still flow and an armed save can still fire.
- Subscription to `saved` is level-triggered. The output lives until OBS resets its output handler (profile switch, Settings > Apply), not until the buffer stops, and lifecycle events can be suppressed, so the manager re-checks which output is current on each lifecycle event, on `FINISHED_LOADING`, on `PROFILE_CHANGED`, on every save request, and on every watchdog tick. It never asks for the output before `FINISHED_LOADING` unless the buffer is active: at module load OBS has not created its output handler, and the frontend API dereferences it unchecked.
- A saved signal with nothing outstanding came from outside the plugin and is logged `no-pending-request` and left alone.

### Residual ambiguity

If a foreign save has been requested but OBS has not yet started writing it when the plugin arms its own, OBS merges both into one `save_ts` and the single file is trimmed as ours. OBS exposes nothing that reveals an armed-but-unstarted `save_ts`; the window is one encoded packet.

### Deferred clips

A deferred request is issued only after the previous file finishes writing, and the trim keeps the last N seconds of the file it produces. The clip therefore ends when the deferred save was issued, not when the key was pressed. On slow storage, where a write can take minutes, the moment the user wanted may fall outside the clip. OBS behaves the same way for its own saves pressed mid-write.

### The watchdog

A `QTimer` runs only while a request is outstanding. It exists for saves that will never produce a `saved` signal at all: a mux pipe failure skips the signal entirely (lines 1120-1134), as does a failed mux thread or a stalled encoder. It does **not** bound how long a write may take.

- Until `Config::SAVE_WATCHDOG_GRACE_MS` has passed since the save was issued, it does nothing. That grace bounds the time from a save to OBS starting the file, which is one encoded packet, not the time to finish writing it.
- While the video encoder is paused it restarts the grace period, because OBS keeps the save armed and fires it on unpause.
- After the grace period it checks, at most every `Config::SAVE_WATCHDOG_PROBE_INTERVAL_MS`, whether OBS has started the file. If it has, the request stays however long the write takes, with a single warning past `Config::SAVE_MUX_STALL_WARN_MS`. If not, the save was lost: `skipped reason=no-mux-started`, and the deferred request is issued.
- If the output has gone inactive and OBS has not started the file, it applies the same handling as `REPLAY_BUFFER_STOPPED`, covering a stop whose event was suppressed.

### Threading

The `saved` callback runs on OBS's mux thread and only reads the path, bumps an atomic counter and posts to the main thread. It must never block: `signal_handler_disconnect()` holds the signal's mutex while waiting for a running callback, so a blocking callback would deadlock shutdown. The same property means that once `shutdown()` has disconnected, no callback is running or can start. The held output reference is only reassigned after disconnecting, which is what makes the callback's read of it safe.

## Save segment flow
1. User clicks a duration button or hotkey. OBS routes hotkey callbacks to the Qt main thread.
2. `ReplayBufferManager::saveSegment(duration, parent)` validates, with a warning dialog on failure:
   - Replay buffer is active.
   - `duration <= currentBufferLength` from `SettingsManager`.
3. `requestSave(...)` runs the pre-flight gate, then arms, folds, defers or waits behind a foreign save as described above. Each transition logs a `SAVE STATE:` line.
4. When OBS finishes the file, the output's `saved` signal fires on the mux thread; the manager reads the path and posts it to `handleSaveCompleted(...)` on the main thread.
5. The manager takes the outstanding request, queues a trim job for its worker thread, then issues any deferred request.

## Save full buffer flow
1. User clicks "Save Replay Buffer".
2. `ReplayBufferManager::saveFullBuffer(...)` checks buffer activity.
3. If active, it requests a save with duration `0`, an explicit do-not-trim marker that goes through the same gate and slots as any other request.
4. The completion carrying that marker is reported `skipped reason=save-full-buffer` and left untrimmed.

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

`OBSBasic::ReplayBufferSaved()` calls `AutoRemux(path)` immediately after firing the saved event, so when Settings → Advanced → "Automatically remux to mp4" is enabled OBS is reading the same file the plugin is about to trim and delete. Antivirus and cloud-sync clients do the same to any newly written file. Both the input open and the original delete retry with backoff, and the AutoRemux setting is logged when a trim starts so the interaction is visible in the log.

## Diagnostics

Every file OBS finishes, and every save request the plugin refuses or abandons, produces exactly one verdict line, prefixed `TRIM VERDICT`:

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
| `encoder-paused` | Refused: recording is paused, and OBS drops replay saves while the encoders are paused |
| `buffer-inactive` | Refused: the replay buffer output was not active |
| `replay-buffer-stopped` | The buffer stopped before OBS started writing the requested file |
| `no-mux-started` | OBS never started a file for the request (watchdog) |
| `output-replaced` | OBS replaced its replay buffer output (profile switch, Settings > Apply) while a request was live |
| `shutdown` | OBS closed while a request was live or a file was waiting to be trimmed |
| `no-saved-path` | OBS reported a save but no path |
| `open-input-failed` | The saved file could not be opened, even after retries |
| `output-too-long` | The cut point collapsed; the clip would have been near full length |
| `output-too-short` / `output-unreadable` | The written clip did not survive verification |
| `no-packets-written` | Nothing was copied; writing would have produced an empty clip |
| `rename-failed` | The verified clip could not take its final name |

Refused and abandoned requests carry `requested=Ns` instead of `file=`, plus `deferred` when it was the deferred request.

Correlation transitions are logged as `SAVE STATE:` lines with a generation number (`armed`, `folded`, `deferred`, `waiting-foreign`, `promoted`, `cleared`), so the pairing of requests and files can be read straight from a log. Pairing OBS's own `Wrote replay buffer to '...'` lines with `TRIM VERDICT` lines checks it independently.

Successes and failures also surface briefly in the OBS status bar via `StatusReporter`. Skipped saves are log-only.

## Third-party compatibility
- [Smart Replay Mover](https://github.com/SlonickLab/Smart-Replay-Mover) (SlonickLab) is a third-party OBS Lua script that waits for this plugin to finish trimming, then moves the final `_trimmed` file into per-game folders. See [issue #23](https://github.com/JoshuaPotter/replay-buffer-pro/issues/23).
- The `_trimmed` suffix and the deletion of the original are unchanged. Trims are now written to a `.rbp-partial.<ext>` scratch file and renamed into place, so the `_trimmed` file appears atomically and is complete the moment it exists — previously a watcher could observe it partially written.
- If the `_trimmed` suffix, the original-file deletion, or the timing of when the final file appears changes, open an issue at https://github.com/SlonickLab/Smart-Replay-Mover so they can update their compatibility handling.

## Error handling
- UI warnings show when the replay buffer is inactive or the requested duration is too long.
- A failed trim leaves the original clip in place, logs a `failed` verdict naming the reason, and shows a status bar message.
- If no saved replay path is returned, trimming is skipped and logged.
- A save OBS would drop (buffer inactive, recording paused) is refused before any state is recorded, with a log-only verdict.

## Key classes and functions
- `ReplayBufferManager::saveSegment(...)`
- `ReplayBufferManager::saveFullBuffer(...)`
- `ReplayBufferManager::requestSave(...)`
- `ReplayBufferManager::checkArmable()`
- `ReplayBufferManager::ensureSubscribed()`
- `ReplayBufferManager::onSavedSignal(...)`
- `ReplayBufferManager::handleSaveCompleted(...)`
- `ReplayBufferManager::promoteDeferred()`
- `ReplayBufferManager::onWatchdogTick()`
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

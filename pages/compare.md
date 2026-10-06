# Replay Buffer Pro vs the built-in OBS replay buffer

The built-in OBS Studio replay buffer saves one fixed length. Replay Buffer Pro adds a button and hotkey for any length, trimmed without re-encoding.

## What is the difference?

The built-in OBS Studio replay buffer saves the whole buffer as one file, so every save is the same length. Replay Buffer Pro is a plugin that works on top of it. You keep one long buffer and choose the clip length at save time, with a button or hotkey for each length.

| Feature | Built-in OBS replay buffer | Replay Buffer Pro |
| --- | --- | --- |
| Clip length per save | Always the buffer length | Any length from 1 second to 6 hours, up to the buffer length |
| Save controls | One Save Replay hotkey | A button and a hotkey for each duration, plus a save-full-buffer button |
| Changing the buffer length | Settings → Output | A numeric field in the dock, while the buffer is stopped |
| Re-encoding | None | None, trimming is a stream copy |
| Disk writes per save | The whole buffer | The whole buffer, then the trimmed clip; the full-length file is deleted |
| Saving from other apps | obs-websocket saves the whole buffer | A [`SaveClip` vendor request](https://joshuapotter.github.io/replay-buffer-pro/usage/#obs-websocket) with any duration (version 1.8.0 and later) |
| Platforms | Everywhere OBS runs | Windows 10/11 (64-bit) and macOS 13.0 or later |
| Requirements | OBS Studio | OBS Studio 32.2.0 or later |
| Cost | Free and open source | Free and open source, GPL v2 or later |

## When is the built-in replay buffer enough?

When you only ever want one clip length, or you save rarely. The built-in buffer needs no install and works on every platform OBS supports, including Linux. Set the length you need and use the Save Replay hotkey.

## When is Replay Buffer Pro the better fit?

When you want clips of several lengths from the same session, for example a 15-second highlight for a short-form video and a 2-minute segment for a longer upload, without opening Settings or a video editor between saves.

## Does Replay Buffer Pro replace the built-in replay buffer?

No. It uses the built-in replay buffer and OBS's own save. Replay Buffer Pro does not capture footage itself. It decides how much of each saved replay to keep. Replays saved with OBS's own Save Replay hotkey, the tray menu, or obs-websocket are not trimmed.

## What about other ways to get shorter clips?

You can trim after the fact in a video editor or with FFmpeg using `-c copy`. It is free and works anywhere, but it is a manual step for every clip. This page compares Replay Buffer Pro only with OBS Studio's own replay buffer. Other capture tools with their own instant-replay features are separate software.

Read the [guide to the OBS replay buffer](https://joshuapotter.github.io/replay-buffer-pro/obs-replay-buffer-guide/) or [install Replay Buffer Pro](https://joshuapotter.github.io/replay-buffer-pro/#install).

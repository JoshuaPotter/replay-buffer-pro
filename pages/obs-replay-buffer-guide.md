# How the OBS replay buffer works, and how to save clips of any length

The built-in replay buffer saves one fixed length. This guide explains how it works and how to get clips of different lengths from the same buffer.

## What is the OBS replay buffer?

The OBS Studio replay buffer keeps a rolling window of your most recent footage in memory. When you save it, OBS writes that footage to disk as a video file. Older footage is overwritten as new footage comes in, so the buffer always holds the last stretch of time you set as its length.

It works like an instant replay. You don't have to start recording before something happens, because the last few seconds or minutes are already captured.

## How do I turn on the replay buffer in OBS Studio?

Open **Settings → Output**, enable the replay buffer, and set the maximum replay time. Then start the replay buffer from OBS. The buffer only captures footage while it is running.

The maximum replay time is the buffer length. With Replay Buffer Pro installed you can also change it from the plugin's dock while the buffer is stopped.

## How do I save a replay in OBS Studio?

Press the Save Replay hotkey, which you set under **Settings → Hotkeys**, or use the Save Replay entry in the OBS tray menu. OBS writes the whole buffer to a file in your Replay Buffer save directory, which is set in the Output settings.

## What are the limits of the built-in replay buffer?

The built-in replay buffer saves one length: the length the buffer is set to. A 10-minute buffer gives you a 10-minute file every time, even when you only wanted the last 30 seconds.

To get a shorter clip you can change the buffer length in Settings before each session, or save the full file and trim it afterwards in a video editor.

## How can I save clips of different lengths in OBS Studio?

Keep one long replay buffer and trim each save to the length you want. [Replay Buffer Pro](https://joshuapotter.github.io/replay-buffer-pro/) is a free, open-source OBS Studio plugin that does this with a button and a hotkey for each length.

1. Set the buffer to the longest clip you will want, for example 10 minutes.
2. Start the replay buffer in OBS.
3. Click a save button such as **Last 30 Seconds**, or press its hotkey.
4. Replay Buffer Pro saves the buffer, trims it to the last 30 seconds, and keeps only the trimmed clip, named with a `_trimmed` suffix.

Each save button can be set to any duration from 1 second to 6 hours. Hotkeys are bound in OBS under **Settings → Hotkeys** by searching for Replay Buffer Pro.

If you would rather not install a plugin, you can save the full replay and cut it in a video editor, or with FFmpeg using `-c copy` to avoid re-encoding. That adds a manual step to every clip.

## Does trimming a replay reduce quality?

No. Replay Buffer Pro trims with a stream copy through FFmpeg's libavformat, the equivalent of `-c copy`. The footage is repackaged rather than re-encoded, so the clip keeps the bitrate, codec, and quality of the original and is ready quickly.

A stream copy can only cut on keyframes, so a very long encoder keyframe interval can make a trim fail. If the OBS log shows `failed reason=output-too-long`, set **Settings → Output → Keyframe Interval** to 2 seconds. A failed trim always leaves the original full-length clip in place.

## How much does saving write to disk?

OBS Studio can only save the whole replay buffer, so every save writes the full buffer first. Replay Buffer Pro then writes the shorter clip and deletes the full-length file. A 15-minute buffer at 40 Mbps is about 4.5 GB per save.

If you save often at high bitrates, keep the buffer length modest or point the Replay Buffer save directory at another drive.

## Which platforms and OBS versions are supported?

Replay Buffer Pro runs on Windows 10/11 (64-bit) and macOS 13.0 or later, on Apple Silicon and Intel, and requires OBS Studio 32.2.0 or later. It is free and licensed GPL v2 or later.

See the [install steps](https://joshuapotter.github.io/replay-buffer-pro/#install), the [comparison with the built-in replay buffer](https://joshuapotter.github.io/replay-buffer-pro/compare/), or the [changelog](https://joshuapotter.github.io/replay-buffer-pro/changelog/).

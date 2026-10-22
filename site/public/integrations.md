# Community integrations

Tools other people have built to work with Replay Buffer Pro.

> These integrations were made by other people. They are not necessarily endorsed, tested, or supported by the maintainer of Replay Buffer Pro, and they can stop working when either project changes. Check each project's own page for its status and support, and test it yourself before relying on it.

## Integrations

### OBS Replay Buffer Pro for Stream Deck

A Stream Deck plugin by @jerptrs, released under the MIT license. It has keys to turn the OBS replay buffer on and off and to save the last 15 seconds, 30 seconds, 60 seconds, 5, 15, or 30 minutes, or a custom length from 1 second to 6 hours. It talks to OBS through obs-websocket using the `SaveClip` request, so it needs Replay Buffer Pro 1.8.0 or later and no OBS hotkeys.

[Stream Deck plugin on GitHub](https://github.com/jerptrs/streamdeck-replay-buffer-pro) · [Announcement](https://github.com/JoshuaPotter/replay-buffer-pro/discussions/49)

### Smart Replay Mover

A free OBS Lua script by SlonickLab that moves saved replays into folders by game. Since version 2.10.0 it detects Replay Buffer Pro, waits for the trim to finish, then moves the final `_trimmed` clip and can optionally remove the suffix.

[Smart Replay Mover on GitHub](https://github.com/SlonickLab/Smart-Replay-Mover) · [Compatibility thread](https://github.com/JoshuaPotter/replay-buffer-pro/issues/23)

## Can I use a Stream Deck with Replay Buffer Pro?

Yes, in a few ways. Bind a **Replay Buffer Pro: Save…** hotkey in OBS under **Settings → Hotkeys** and have the Stream Deck send that key combination, use the community Stream Deck plugin above, which sends obs-websocket `SaveClip` requests and needs version 1.8.0 or later, or use another tool that sends obs-websocket requests.

The official Stream Deck OBS Studio plugin's Save Replay Buffer action saves the full buffer without trimming, because the save comes from outside Replay Buffer Pro.

## Can other apps save clips through obs-websocket?

Yes. Since version 1.8.0, any obs-websocket client can send a `SaveClip` request for a clip of any whole-second length from 1 to 21600. See [how to use Replay Buffer Pro](https://joshuapotter.github.io/replay-buffer-pro/usage/#obs-websocket) for the request, the response format, and examples.

## What do file-organizing tools need to know about the output files?

Each trimmed clip appears as `<name>_trimmed.<ext>` next to where OBS saved the replay. It is written to a `.rbp-partial.<ext>` scratch file first and renamed into place, so the `_trimmed` file is complete the moment it exists. After that, the original full-length file is deleted. Tools that move or rename replays should wait for the `_trimmed` file rather than acting on the original.

If the `_trimmed` suffix, the original-file deletion, or the timing of the final file changes, the developer documentation asks contributors to open an issue at Smart Replay Mover so its author can update.

## How do I get my integration listed here?

Open a thread in [GitHub Discussions](https://github.com/JoshuaPotter/replay-buffer-pro/discussions) with a link to your project, what it does, and which Replay Buffer Pro version you have used it with. Listings are informational and don't imply endorsement or testing.

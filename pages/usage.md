# How to use Replay Buffer Pro

Every way to save a clip, and what each one does.

## Ways to save a clip

Replay Buffer Pro trims clips three ways: dock buttons, hotkeys, and an obs-websocket request. All three use the same save-and-trim path. Saves that come from outside the plugin are left at full length.

| Interface | Best for | Trimmed to your duration | Needs |
| --- | --- | --- | --- |
| Dock save buttons | Clicking a length while OBS is in front | Yes | Replay buffer running |
| Hotkeys | Clipping mid-game without leaving it | Yes | A binding for each **Replay Buffer Pro: Save …** hotkey |
| obs-websocket `SaveClip` | Scripts, Stream Deck plugins, and other apps | Yes, any whole number of seconds from 1 to 21600 | obs-websocket server turned on |
| Save Replay Buffer button | Keeping the whole buffer | No, saved at full buffer length | Replay buffer running |
| OBS's own Save Replay (hotkey, tray menu, `SaveReplayBuffer` request) | Nothing in this plugin; it is OBS's save | No, logged as `skipped reason=no-pending-request` | Replay buffer running |

## What do I need before I save?

The replay buffer must be running. Replay Buffer Pro does not capture footage; it trims what OBS saves from the buffer, so start the buffer from OBS Studio's controls first.

Recording must not be paused. OBS drops replay saves while recording is paused, so the plugin refuses the save up front and logs `encoder-paused`. A clip also cannot be longer than the buffer. Set the buffer to the longest clip you will want, for example 10 minutes.

## How do I use the dock?

Open **Docks → Replay Buffer Pro** in OBS Studio. The dock has four parts.

- **Maximum Replay Buffer Length.** A numeric field from 1 second to 6 hours. It writes the same OBS profile setting as **Settings → Output**. It is disabled while the replay buffer is running, so stop the buffer to change it.
- **Save Clips.** Six buttons, **Last 15 Seconds**, **Last 30 Seconds**, **Last 1 Minute**, **Last 5 Minutes**, **Last 15 Minutes** and **Last 30 Minutes** by default. A button is disabled when the buffer is shorter than its duration.
- **Customize.** Opens a dialog where each button can be any whole number of seconds from 1 to 21600. The durations are shared across scene collections and profiles, and the hotkey names follow them.
- **Save Replay Buffer.** Saves the whole buffer without trimming.

## How do I save with hotkeys?

Open **Settings → Hotkeys** in OBS Studio and search for **Replay Buffer Pro**. Each save button has its own hotkey, named after its duration, such as **Replay Buffer Pro: Save 30 Seconds**. Bindings are stored in `hotkey_bindings.json` in the plugin's config folder.

The Save Replay Buffer button has no hotkey of its own. To save the full buffer from a key, use OBS's built-in **Save Replay** hotkey, which also saves it untrimmed.

## How do I save clips over obs-websocket?

Turn on the server under **Tools → WebSocket Server Settings**, connect with any obs-websocket v5 client, and send a `CallVendorRequest` with vendor `replay-buffer-pro` and request `SaveClip`. It uses the same save-and-trim path as the dock buttons.

```json
{
  "op": 6,
  "d": {
    "requestType": "CallVendorRequest",
    "requestId": "save-clip-1",
    "requestData": {
      "vendorName": "replay-buffer-pro",
      "requestType": "SaveClip",
      "requestData": { "durationSeconds": 120 }
    }
  }
}
```

With [obs-websocket-js](https://github.com/obs-websocket-community-projects/obs-websocket-js), the same call looks like this. The plugin's reply is the `responseData` field of the result.

```ts
import OBSWebSocket from 'obs-websocket-js';

const obs = new OBSWebSocket();
await obs.connect('ws://127.0.0.1:4455', 'your-password');

const { responseData: clip } = await obs.call('CallVendorRequest', {
  vendorName: 'replay-buffer-pro',
  requestType: 'SaveClip',
  requestData: { durationSeconds: 120 },
});

console.log(clip); // { accepted: true, durationSeconds: 120, clamped: false }
```

`durationSeconds` must be a whole number from 1 to 21600. Anything else is rejected as `invalid-duration`. The plugin answers with one of these results. Because it is an application-level result, the obs-websocket request itself still succeeds when `accepted` is `false`.

| Result | Meaning |
| --- | --- |
| `{"accepted": true, "durationSeconds": 120, "clamped": false}` | The save started. `durationSeconds` is the length being saved. `clamped` is `true` when the buffer was shorter than requested. |
| `{"accepted": false, "error": "invalid-duration"}` | The duration is missing, not a number, not whole, or outside 1 to 21600. |
| `{"accepted": false, "error": "buffer-inactive"}` | The replay buffer is not running. |
| `{"accepted": false, "error": "save-refused"}` | OBS would drop the save, for example because recording is paused. |
| `{"accepted": false, "error": "unavailable"}` | OBS is shutting down. |
| `{"accepted": false, "error": "timeout"}` | OBS was busy and did not start the save within 2 seconds. Nothing was saved. |

A request longer than the buffer saves the whole buffer instead of failing, and the result has `"clamped": true`. The dock buttons refuse in that case instead.

Accepted means the save started, not that the file exists. The request returns without waiting for the file, and trimming runs in the background once OBS has written the replay. Wait for the `_trimmed` file described below. If obs-websocket is not available, the plugin logs a warning at load and the dock and hotkeys work as usual.

## Can I use a Stream Deck or other tools?

Yes, in two ways. Bind a **Replay Buffer Pro: Save …** hotkey and have the tool send that key combination, or have it send `SaveClip` over obs-websocket, which needs Replay Buffer Pro 1.8.0 or later and no OBS hotkeys.

The official Stream Deck OBS Studio plugin's Save Replay Buffer action saves the full buffer without trimming, because the save comes from outside Replay Buffer Pro. See [community integrations](https://joshuapotter.github.io/replay-buffer-pro/integrations/) for tools built for this plugin.

## Where do the clips go?

Each trimmed clip is saved as `<name>_trimmed.<ext>` next to the replay OBS wrote, in your Replay Buffer save directory. It is written to a `.rbp-partial.<ext>` file first and renamed into place, so the `_trimmed` file is complete the moment it exists. Then the original full-length file is deleted.

A failed trim keeps the original. Replays saved outside the plugin, and Save Replay Buffer clicks, stay full length with no `_trimmed` file. Every save writes one `TRIM VERDICT` line to the OBS log that says what happened. The [troubleshooting section](https://joshuapotter.github.io/replay-buffer-pro/#troubleshooting) lists the reasons.

## What happens if I press save twice?

A save pressed while OBS is still writing an earlier clip waits for that write to finish, then is trimmed so it ends at the moment you pressed it. Time with recording paused does not count. If the buffer no longer holds anything from before your press, the trim fails with `window-not-in-buffer` and the full-length file is kept.

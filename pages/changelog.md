# Replay Buffer Pro changelog

What changed in each release of Replay Buffer Pro. Full release notes and downloads: https://github.com/JoshuaPotter/replay-buffer-pro/releases

## 1.8.0 (2026-10-04)

Save clips from other apps through obs-websocket.

- **Save clips from other apps.** obs-websocket clients can save a clip of any whole-second length from 1 second to 6 hours with `CallVendorRequest`, using vendor `replay-buffer-pro`, request `SaveClip`, and data `{"durationSeconds": 120}`. Clips go through the same save-and-trim path as the dock buttons.
- **Longer than the buffer.** If a request asks for more than the buffer holds, the whole buffer is saved and the response includes `"clamped": true`.

## 1.7.0 (2026-09-25)

More reliable trimming on slow storage, and clips that end when you press save.

- **Reliable trimming on slow storage.** Clips are no longer left untrimmed when OBS takes longer than 30 seconds to write the file. Each save is matched to a file the way OBS produces them, with no time limit on writes.
- **Clips end when you press save.** A save pressed while OBS is still writing an earlier clip waits for that write, then is trimmed to end at the moment you pressed it. Time with recording paused is excluded.
- **Dropped saves are refused up front.** When the replay buffer is off or recording is paused, the save is refused and logged right away.
- **Saves from outside the plugin are left alone.** Files saved with OBS's own Save Replay hotkey, the tray menu, or obs-websocket are never mistaken for plugin clips.
- **Clearer logs.** New `TRIM VERDICT` reasons and `SAVE STATE` lines explain what happened to every save.

## 1.6.0 (2026-08-11)

Trimming reliability fixes and a dock that looks more like a native OBS dock.

- **Reliable replay trimming.** Saves are matched through an expiring request queue instead of a single slot, so overlapping saves or dropped requests can no longer apply the wrong trim duration or skip trimming. Fixed a keyframe cut-point bug that could collapse a trim to the full buffer length while still logging success. Trims are committed through a verified temporary file.
- **Improved trim logging.** A `TRIM VERDICT` line is logged for every save, so a bad clip can be diagnosed from the OBS log alone.
- **Dock refresh.** The dock sits in a padded, bordered frame matching built-in OBS docks, and the buffer length slider is replaced by a spinbox.
- **Smaller Windows download.** Debug symbols are no longer included in the release ZIP.

## 1.5.1 (2026-07-23)

Compatibility fix for OBS Studio 32.2.0.

- **Rebuilt against OBS Studio 32.2.0 and FFmpeg 8.** Version 1.5.0 failed to load on OBS Studio 32.2.0 because it linked against FFmpeg libraries that OBS no longer ships.
- **New minimum OBS version: 32.2.0.** This build does not load on older OBS Studio versions. Stay on 1.5.0 if you are not ready to update OBS.

## 1.5.0 (2026-06-28)

macOS support and a new Windows install location.

- **macOS support.** A universal binary for Apple Silicon and Intel, distributed as a `.pkg` installer.
- **Clearer hotkey labels.** Save-clip hotkeys are prefixed with the plugin name, for example `Replay Buffer Pro: Save 30 Seconds`.
- **Windows install path changed** to `C:\ProgramData\obs-studio\plugins\replay-buffer-pro\`. Delete the old DLL from `C:\Program Files\obs-studio\obs-plugins\64bit\` before installing, or OBS loads two copies of the plugin.

## 1.4.0 (2026-02-28)

Customizable save button durations.

- **Customizable save buttons.** Set each save button to the duration you want.

## 1.3.0 (2026-02-06)

Multi-track output support and a smaller download.

- **Multi-track output support.**
- **Smaller plugin.** The bundled FFmpeg dependency was removed.
- **Asynchronous trimming** and a fix for corrupted frames.

## 1.2.0 (2025-10-10)

Fixes a crash on OBS 32.0.x.

- **Crash fix.** Removed the deprecated `obs_frontend_add_dock` call that crashed the plugin on OBS 32.0.x.

## 1.1.0 (2025-03-17)

Interface polish.

- Added ticks under the replay buffer length slider for quickly setting precise values.
- Improved the Save Clips buttons and the buffer length input field.
- Adjusted labels.

## 1.0.0 (2025-03-06)

Initial release.

- First public release of Replay Buffer Pro.

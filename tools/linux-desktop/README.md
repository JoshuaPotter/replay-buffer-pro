# Linux test desktop

An Ubuntu 24.04 x86_64 desktop in Docker, opened in your browser, for testing the plugin's Linux build without a Linux machine. It runs OBS Studio from the official PPA with the plugin's `.deb` installed, the same way a user on Ubuntu installs it. It works on Apple Silicon Macs, where an ARM Linux VM can't install the x86_64 `.deb` or get a new enough OBS.

It is for checking behaviour, not performance: x86 is emulated and OBS renders in software.

## Requirements

- Docker (Docker Desktop on macOS or Windows).
- On Apple Silicon, turn on **Settings → General → Use Rosetta for x86_64/amd64 emulation on Apple Silicon** in Docker Desktop. It works without it, but more slowly.

## Use

From the repository root:

```bash
tools/linux-desktop/run.sh deb   # build release/replay-buffer-pro-<version>-x86_64-linux-gnu.deb
tools/linux-desktop/run.sh up    # start the desktop and open it in the browser
```

The first run builds the Docker image, which takes several minutes. After that, `up` starts in seconds.

The desktop is at <http://localhost:6080/vnc.html?autoconnect=1&resize=scale>. OBS opens on it. Closing OBS restarts it, which is how you test that settings survive a restart. Right-click the desktop for a terminal.

| Command | What it does |
|---|---|
| `run.sh up` | Start the desktop. It installs the newest `release/*.deb` each time, so rebuild with `deb` and run `up` again to test a change. |
| `run.sh deb` | Build the `.deb` and its `-dbgsym.ddeb` inside the image with `.github/scripts/build-ubuntu`. |
| `run.sh shell` | A shell in the running desktop as the `ubuntu` user, who has passwordless `sudo`. |
| `run.sh down` | Stop and remove the desktop. |
| `run.sh image` | Rebuild the image after changing this folder. |

## What's where

- Recordings and replays: `build_linux-desktop/recordings/` on your machine (`/recordings` in OBS).
- OBS settings, plugin settings and logs: `build_linux-desktop/config/` (`~/.config/obs-studio` in the desktop). Logs are in its `logs/` folder, and the plugin's `hotkey_bindings.json` and `properties.json` are in `plugin_config/replay-buffer-pro/`. Delete the folder to start OBS from scratch.
- obs-websocket: `localhost:4455`, once you turn the server on in **Tools → WebSocket Server Settings**.

Both folders are under `build_*/`, so git ignores them.

## Starting settings

On a fresh `config/` folder, OBS skips its auto-configuration wizard and starts with settings that stay smooth under emulation: a 1280×720 canvas at 30 fps, Advanced output mode with x264 at `ultrafast` and a 2-second keyframe interval, MKV recordings to `/recordings`, and the Replay Buffer on at 60 seconds. Change them in OBS as usual; they are kept. The keyframe interval matters: the plugin cuts clips at a keyframe, and x264's own default of about 8 seconds makes clips run long.

The scene has two Media Sources fed by FFmpeg's test generators: **Test pattern** (moving bars with a running timecode, so you can see where a clip starts and ends) and **Test tone** (a 440 Hz tone, so clips have real audio). The dock is under **Docks → Replay Buffer Pro**. `ffprobe` is installed for checking clip lengths:

```bash
ffprobe -v error -show_entries format=duration -of csv=p=0 /recordings/<file>
```

## Limits

- The display is X11 (Xvfb), so hotkeys behave as in an Xorg session. Wayland isn't covered.
- There is no audio device. OBS still records its silent audio mix, so clips have an audio track.
- Keyboard shortcuts pass through the browser, which keeps a few for itself (for example Cmd+W on macOS). Bind hotkeys with Ctrl and Alt.

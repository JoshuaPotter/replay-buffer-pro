#!/usr/bin/env bash
# Desktop session for the Flatpak test desktop: a virtual X display served to the browser
# through noVNC, with Flatpak OBS restarted whenever it quits. Runs as the "ubuntu" user.
set -euo pipefail

export DISPLAY=:1
export LIBGL_ALWAYS_SOFTWARE=1

Xvfb "${DISPLAY}" -screen 0 "${RESOLUTION:-1600x900}x24" -nolisten tcp &
for _ in {1..50}; do [[ -S /tmp/.X11-unix/X1 ]] && break; sleep 0.2; done
if [[ ! -S /tmp/.X11-unix/X1 ]]; then
  echo "Xvfb didn't start (RESOLUTION=${RESOLUTION:-1600x900})." >&2
  exit 1
fi

openbox &
x11vnc -display "${DISPLAY}" -forever -shared -nopw -localhost -rfbport 5900 -quiet &
websockify --web /usr/share/novnc 6080 localhost:5900 >/dev/null 2>&1 &

# First start only: the Linux test desktop's starting settings (720p30, x264 ultrafast,
# replay buffer on, saves to /recordings), in Flatpak OBS's own config folder.
config="${HOME}/.var/app/com.obsproject.Studio/config/obs-studio"
if [[ ! -e "${config}/user.ini" ]]; then
  mkdir -p "${config}"
  cp -R /opt/rbp/obs-config/. "${config}/"
fi

# Install the newest bundle from build_flatpak/ the way a user does, into this user's installation
bundle="$(ls /plugin/replay-buffer-pro-*-x86_64.flatpak 2>/dev/null | sort -V | tail -n 1 || true)"
if [[ -n "${bundle}" ]]; then
  flatpak install --user -y --noninteractive "${bundle}"
else
  echo "No replay-buffer-pro .flatpak in build_flatpak/; OBS starts without the plugin." >&2
fi

echo "Desktop ready (run.sh up prints its address)"
# Clear OBS's crash sentinel before each launch, or OBS opens at its Safe Mode prompt
# and Safe Mode turns the plugin off.
while true; do
  rm -f "${config}"/.sentinel/run_*
  dbus-run-session -- flatpak run com.obsproject.Studio || true
  sleep 1
done

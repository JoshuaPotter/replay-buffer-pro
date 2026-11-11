#!/usr/bin/env bash
# Desktop session: a virtual X display served to the browser through noVNC, with
# OBS restarted whenever it quits, so "restart OBS" is just closing its window.
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

# First start only: skip the auto-configuration wizard and use settings that stay
# smooth under x86 emulation (720p30, x264 ultrafast, replay buffer on, saves to /recordings).
if [[ ! -e "${HOME}/.config/obs-studio/user.ini" ]]; then
  cp -R /opt/rbp/obs-config/. "${HOME}/.config/obs-studio/"
fi

echo "Desktop ready at http://localhost:6080/vnc.html?autoconnect=1&resize=scale"
# run.sh down always kills OBS, so clear OBS's crash sentinel before each launch.
# Otherwise OBS opens at its Safe Mode prompt, and Safe Mode turns the plugin off.
while true; do
  rm -f "${HOME}"/.config/obs-studio/.sentinel/run_*
  obs || true
  sleep 1
done

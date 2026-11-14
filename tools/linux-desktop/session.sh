#!/usr/bin/env bash
# Desktop session: a virtual X display served to the browser through noVNC, with
# OBS restarted whenever it quits, so "restart OBS" is just closing its window.
set -euo pipefail

. /opt/rbp/start-display.sh

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

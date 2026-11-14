#!/usr/bin/env bash
# Desktop session for the Flatpak test desktop: the Linux test desktop's virtual X display
# served to the browser through noVNC, with Flatpak OBS restarted whenever it quits. Runs as
# the "ubuntu" user.
set -euo pipefail

. /opt/rbp/start-display.sh

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

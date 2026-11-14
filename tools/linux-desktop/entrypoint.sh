#!/usr/bin/env bash
# Installs the newest plugin .deb for this image's Ubuntu release from /plugin (the
# repo's release/ folder), then starts the desktop session as the "ubuntu" user.
set -euo pipefail

. /etc/os-release
deb="$(ls /plugin/replay-buffer-pro-*-x86_64-ubuntu${VERSION_ID}-linux-gnu.deb 2>/dev/null | sort -V | tail -n 1 || true)"
if [[ -n "${deb}" ]]; then
  dpkg -i "${deb}"
else
  echo "No replay-buffer-pro .deb in release/; OBS starts without the plugin." >&2
fi

chown ubuntu:ubuntu /home/ubuntu/.config /home/ubuntu/.config/obs-studio /recordings
exec runuser -u ubuntu -- /opt/rbp/session.sh

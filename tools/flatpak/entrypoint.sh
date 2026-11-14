#!/usr/bin/env bash
# Starts the system D-Bus that Flatpak needs, gives the folders Docker created as root for
# the mounts to the "ubuntu" user, and starts the desktop session as that user.
set -euo pipefail

mkdir -p /run/dbus
dbus-daemon --system --fork

app=/home/ubuntu/.var/app/com.obsproject.Studio
chown ubuntu:ubuntu /home/ubuntu/.var /home/ubuntu/.var/app "${app}" "${app}/config" "${app}/config/obs-studio" /recordings
exec runuser -u ubuntu -- /opt/rbp/session.sh

# Sourced by the session.sh of both test desktops (this folder's and tools/flatpak/'s): starts
# a virtual X display served to the browser through noVNC on port 6080, and exports DISPLAY.

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

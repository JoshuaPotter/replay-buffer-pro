#!/usr/bin/env bash
# Linux test desktop for Replay Buffer Pro. See README.md next to this file.
#
# Usage: tools/linux-desktop/run.sh [up|deb|down|shell|image]
#   up     Start the desktop (installs the newest release/*.deb) and open it in the browser
#   deb    Build release/replay-buffer-pro-<version>-x86_64-linux-gnu.deb inside the image
#   down   Stop and remove the desktop container
#   shell  Open a shell in the running desktop as the "ubuntu" user
#   image  Rebuild the Docker image (after editing this folder)
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "${here}/../.." && pwd)"
image=replay-buffer-pro-linux-desktop
name=rbp-linux-desktop
state="${root}/build_linux-desktop"
url='http://localhost:6080/vnc.html?autoconnect=1&resize=scale'

build_image() { docker build --platform linux/amd64 -t "${image}" "${here}"; }
ensure_image() { docker image inspect "${image}" >/dev/null 2>&1 || build_image; }

case "${1:-up}" in
  image)
    build_image
    ;;
  deb)
    ensure_image
    # Your user, not root, so build_x86_64/ and release/ stay yours on Linux hosts.
    docker run --rm --platform linux/amd64 --user "$(id -u):$(id -g)" -e HOME=/tmp \
      --entrypoint bash -v "${root}:/src" -w /src "${image}" -c \
      '.github/scripts/build-ubuntu --target ubuntu-x86_64 --skip-deps && cmake --build build_x86_64 --config RelWithDebInfo -t package'
    ;;
  up)
    ensure_image
    mkdir -p "${state}/config" "${state}/recordings" "${root}/release"
    docker rm -f "${name}" >/dev/null 2>&1 || true
    docker run -d --name "${name}" --platform linux/amd64 --shm-size=1g \
      -p 127.0.0.1:6080:6080 -p 127.0.0.1:4455:4455 \
      -v "${root}/release:/plugin:ro" \
      -v "${state}/config:/home/ubuntu/.config/obs-studio" \
      -v "${state}/recordings:/recordings" \
      "${image}" >/dev/null
    waited=0
    until curl -fs -o /dev/null http://localhost:6080/vnc.html; do
      if [[ "$(docker inspect -f '{{.State.Running}}' "${name}" 2>/dev/null)" != true ]] || (( ++waited > 120 )); then
        docker logs "${name}" >&2 || true
        echo "The desktop didn't start; its log is above." >&2
        exit 1
      fi
      sleep 1
    done
    echo "Desktop: ${url}"
    echo "Recordings: ${state}/recordings"
    echo "OBS settings and logs: ${state}/config"
    if command -v open >/dev/null; then open "${url}"; elif command -v xdg-open >/dev/null; then xdg-open "${url}"; fi
    ;;
  down)
    docker rm -f "${name}" >/dev/null 2>&1 || true
    ;;
  shell)
    docker exec -it -u ubuntu -e DISPLAY=:1 -w /home/ubuntu "${name}" bash
    ;;
  *)
    sed -n '4,9p' "$0" >&2
    exit 2
    ;;
esac

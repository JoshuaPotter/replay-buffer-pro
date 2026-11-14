# Sourced by the run.sh of both test desktops (this folder's and tools/flatpak/'s).

# wait_for_desktop CONTAINER PORT: waits up to two minutes for the container's noVNC page on
# localhost:PORT, and exits with the container's log if it stops or never serves it.
wait_for_desktop() {
  local container=$1 port=$2 waited=0
  until curl -fs -o /dev/null "http://localhost:${port}/vnc.html"; do
    if [[ "$(docker inspect -f '{{.State.Running}}' "${container}" 2>/dev/null)" != true ]] || (( ++waited > 120 )); then
      docker logs "${container}" >&2 || true
      echo "The desktop didn't start; its log is above." >&2
      exit 1
    fi
    sleep 1
  done
}

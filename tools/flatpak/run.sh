#!/usr/bin/env bash
# Flathub extension for Replay Buffer Pro. See README.md next to this file.
#
# Usage: tools/flatpak/run.sh <command> [argument]
#   tarball           Make the source tarball from HEAD the way CI does, in build_flatpak/
#   build [tarball]   Build build_flatpak/replay-buffer-pro-<version>-x86_64.flatpak from a source
#                     tarball (default: the newest one in build_flatpak/ or release/)
#   up                Start a desktop running Flatpak OBS with the newest .flatpak installed, open it
#   manifest VERSION  Write the files for flathub/flathub, filled in from that published release
#   lint              Run Flathub's linter on those files
#   down              Stop and remove the desktop
#   shell             Open a shell in the running desktop as the "ubuntu" user
#   image             Rebuild the Docker image (after editing this folder)
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "${here}/../.." && pwd)"
id=com.obsproject.Studio.Plugin.ReplayBufferPro
image=replay-buffer-pro-flatpak
name=rbp-flatpak-desktop
volume=rbp-flatpak
state="${root}/build_flatpak"
# Not the Linux test desktop's 6080 and 4455, so both can run at once
url='http://localhost:6081/vnc.html?autoconnect=1&resize=scale'

build_image() {
  docker buildx build --platform linux/amd64 --build-context "linux-desktop=${root}/tools/linux-desktop" \
    -t "${image}" --load "${here}"
}
ensure_image() { docker image inspect "${image}" >/dev/null 2>&1 || build_image; }

# x86_64 emulated on an ARM host can't load bwrap's seccomp filter, so leave it out there.
emulated=()
if [[ "$(uname -m)" != x86_64 ]]; then emulated=(-e FLATPAK_BWRAP=/opt/rbp/bwrap-no-seccomp); fi

# Flatpak and its runtimes live in a Docker volume: OSTree needs a Linux filesystem.
in_container() {
  docker run --rm --platform linux/amd64 --privileged ${emulated[@]+"${emulated[@]}"} -v "${volume}:/var/lib/flatpak" \
    -v "${here}:/manifest:ro" -v "${state}:/state" "$@"
}

case "${1:-}" in
  image)
    build_image
    ;;
  tarball)
    # CI makes it with CPack's package_source from a clean checkout; git archive gives one.
    # The Linux test desktop's default (26.04) image does the build.
    deb_image=replay-buffer-pro-linux-desktop-26.04
    docker image inspect "${deb_image}" >/dev/null 2>&1 || "${root}/tools/linux-desktop/run.sh" image
    rm -rf "${state}/source"
    mkdir -p "${state}/source"
    git -C "${root}" archive HEAD | tar -x -C "${state}/source"
    docker run --rm --platform linux/amd64 --user "$(id -u):$(id -g)" -e HOME=/tmp --entrypoint bash \
      -v "${state}/source:/src" -w /src "${deb_image}" -c \
      'cmake --preset ubuntu-x86_64 >/dev/null && cmake --build build_x86_64 -t package_source'
    mv "${state}"/source/release/replay-buffer-pro-*-source.tar.xz "${state}/"
    rm -rf "${state}/source"
    ls "${state}"/replay-buffer-pro-*-source.tar.xz
    ;;
  build)
    ensure_image
    tarball="${2:-$(ls "${state}"/replay-buffer-pro-*-source.tar.xz "${root}"/release/replay-buffer-pro-*-source.tar.xz 2>/dev/null | sort -V | tail -n 1 || true)}"
    if [[ ! -f "${tarball}" ]]; then
      echo "No source tarball. Make one with: tools/flatpak/run.sh tarball" >&2
      exit 1
    fi
    tarball="$(cd "$(dirname "${tarball}")" && pwd)/$(basename "${tarball}")"
    mkdir -p "${state}"
    base="$(basename "${tarball}")"
    in_container -v "${tarball}:/src/${base}:ro" --entrypoint bash "${image}" -c \
      "set -o pipefail; /manifest/build-bundle.sh '/src/${base}' /state /var/lib/flatpak/rbp-work 2>&1 | tee /state/build.log"
    echo "Build log: ${state}/build.log"
    ;;
  manifest)
    ensure_image
    version="${2:?Usage: run.sh manifest VERSION (a published release, such as 1.11.0)}"
    mkdir -p "${state}"
    in_container --entrypoint /manifest/release-manifest.sh "${image}" "${version}" /state/submission
    echo "Files for the Flathub submission: ${state}/submission"
    ;;
  lint)
    ensure_image
    if [[ ! -d "${state}/submission" ]]; then
      echo "No submission files. Write them with: tools/flatpak/run.sh manifest VERSION" >&2
      exit 1
    fi
    # The linter runs in its own sandbox, which can't see /state; lint a copy in the volume.
    in_container --entrypoint bash "${image}" -c "
      set -euo pipefail
      flatpak install --system -y --noninteractive flathub org.flatpak.Builder >/dev/null
      rm -rf /var/lib/flatpak/rbp-lint && cp -R /state/submission /var/lib/flatpak/rbp-lint && cd /var/lib/flatpak/rbp-lint
      mkdir -p /run/dbus && dbus-daemon --system --fork
      lint() { dbus-run-session -- flatpak run --command=flatpak-builder-lint org.flatpak.Builder \"\$@\" 2>&1 | grep -v 'dbus-daemon\[' || true; }
      echo '== manifest'; lint manifest ${id}.yml
      echo '== metainfo'; lint appstream ${id}.metainfo.xml
    "
    ;;
  up)
    ensure_image
    mkdir -p "${state}/config" "${state}/recordings"
    docker rm -f "${name}" >/dev/null 2>&1 || true
    docker run -d --name "${name}" --platform linux/amd64 --privileged ${emulated[@]+"${emulated[@]}"} --shm-size=1g \
      -p 127.0.0.1:6081:6080 -p 127.0.0.1:4456:4455 \
      -v "${volume}:/var/lib/flatpak" -v "${state}:/plugin:ro" \
      -v "${state}/config:/home/ubuntu/.var/app/com.obsproject.Studio/config/obs-studio" \
      -v "${state}/recordings:/recordings" \
      "${image}" >/dev/null
    waited=0
    until curl -fs -o /dev/null http://localhost:6081/vnc.html; do
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
    sed -n '4,14p' "$0" >&2
    exit 2
    ;;
esac

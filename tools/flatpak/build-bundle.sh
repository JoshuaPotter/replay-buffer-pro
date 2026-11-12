#!/usr/bin/env bash
# Builds replay-buffer-pro-<version>-x86_64.flatpak, a single-file bundle of the Flatpak OBS
# extension, from a source tarball made by CPack's package_source. Needs flatpak and
# flatpak-builder on x86_64 Linux. CI's flatpak job and run.sh build both run it.
#
# Usage: build-bundle.sh SOURCE_TARBALL OUT_DIR [WORK_DIR]
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
id=com.obsproject.Studio.Plugin.ReplayBufferPro
tarball="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
out="$(mkdir -p "$2" && cd "$2" && pwd)"
work="$(mkdir -p "${3:-${PWD}/.flatpak-work}" && cd "${3:-${PWD}/.flatpak-work}" && pwd)"
version="$(basename "${tarball}" | sed -E 's/^replay-buffer-pro-(.+)-source\.tar\.xz$/\1/')"
sdk="$(sed -n 's/^sdk: *//p' "${here}/${id}.yml")"

# The SDK, and Flatpak OBS as the runtime the extension builds against
flatpak remote-add --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
flatpak install -y --noninteractive flathub "${sdk}" com.obsproject.Studio//stable

# The manifest builds from a release's tarball URL; point it at this local tarball instead
mkdir -p "${work}/manifest"
sed -e "s|@VERSION@|${version}|g" \
  -e "s|@SHA256@|$(sha256sum "${tarball}" | cut -d' ' -f1)|" \
  -e "s|url: https://github.com/.*-source.tar.xz|path: ${tarball}|" \
  "${here}/${id}.yml" >"${work}/manifest/${id}.yml"
sed -e "s|@VERSION@|${version}|" -e "s|@DATE@|$(date -u +%F)|" \
  "${here}/${id}.metainfo.xml" >"${work}/manifest/${id}.metainfo.xml"

# rofiles-fuse needs FUSE, which containers usually lack
flatpak-builder --force-clean --disable-rofiles-fuse --repo="${work}/repo" --state-dir="${work}/state" \
  "${work}/build" "${work}/manifest/${id}.yml"
flatpak build-bundle --runtime "${work}/repo" "${out}/replay-buffer-pro-${version}-x86_64.flatpak" "${id}" stable
echo "${out}/replay-buffer-pro-${version}-x86_64.flatpak"

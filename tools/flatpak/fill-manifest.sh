#!/usr/bin/env bash
# Writes the manifest, metainfo and flathub.json with their placeholders filled in.
# The archive source is the release's source tarball URL, or TARBALL, a local file, if given.
# build-bundle.sh and run.sh manifest both run it.
#
# Usage: fill-manifest.sh VERSION SHA256 DATE OUT_DIR [TARBALL]
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
id=com.obsproject.Studio.Plugin.ReplayBufferPro
version=$1 sha=$2 date=$3 out=$4 tarball=${5:-}

mkdir -p "${out}"
source="url: https://github.com/JoshuaPotter/replay-buffer-pro/releases/download/${version}/replay-buffer-pro-${version}-source.tar.xz"
if [[ -n "${tarball}" ]]; then source="path: ${tarball}"; fi
sed -e "s|@SOURCE@|${source}|" -e "s|@SHA256@|${sha}|" "${here}/${id}.yml" >"${out}/${id}.yml"
sed -e "s|@VERSION@|${version}|" -e "s|@DATE@|${date}|" "${here}/${id}.metainfo.xml" >"${out}/${id}.metainfo.xml"
cp "${here}/flathub.json" "${out}/"

#!/usr/bin/env bash
# Writes the Flathub submission files for a published release to OUT_DIR, with its source
# tarball's URL, sha256 and publish date filled in. run.sh manifest runs it in the image.
#
# Usage: release-manifest.sh VERSION OUT_DIR
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
version=$1 out=$2

# One read of the release: its date, and the tarball's digest (GitHub records one per asset) and URL
read -r date sha url < <(curl -fsSL "https://api.github.com/repos/JoshuaPotter/replay-buffer-pro/releases/tags/${version}" |
  python3 -c '
import json, sys
release = json.load(sys.stdin)
assets = [a for a in release["assets"] if a["name"].endswith("-source.tar.xz")]
if not assets:
    sys.exit("Release " + release["tag_name"] + " has no -source.tar.xz asset")
digest = (assets[0].get("digest") or "").removeprefix("sha256:")
print(release["published_at"][:10], digest or "-", assets[0]["browser_download_url"])
') || true
[[ -n "${date:-}" ]] || exit 1
if [[ "${sha}" == - ]]; then sha="$(curl -fsSL "${url}" | sha256sum | cut -d' ' -f1)"; fi

rm -rf "${out}"
"${here}/fill-manifest.sh" "${version}" "${sha}" "${date}" "${out}"
grep -n -A2 'type: archive' "${out}"/*.yml

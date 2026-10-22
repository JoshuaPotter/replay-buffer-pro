#!/usr/bin/env bash
# Prints the full GitHub release body for a tagged version: the hand-written
# notes, then the fixed Installation and Requirements sections, then the
# checksums block CI wrote into the draft release.
#
# Usage: compose-release-body.sh VERSION NOTES_FILE
set -euo pipefail

version="$1"
notes="$2"

# Requirements come from the tagged commit, not from whatever is checked out.
obs="$(git show "${version}:buildspec.json" | jq -r '.dependencies["obs-studio"].version')"
macos="$(git show "${version}:CMakePresets.json" \
  | jq -r '.configurePresets[] | select(.name == "macos") | .cacheVariables.CMAKE_OSX_DEPLOYMENT_TARGET')"
checksums="$(gh release view "$version" --json body --jq .body | tr -d '\r' | sed -n '/^### Checksums/,$p')"

if [[ -z "$checksums" ]]; then
  echo "error: the ${version} release has no checksums block yet; wait for the tag build" >&2
  exit 1
fi

tr -d '\r' < "$notes"
echo
sed -e "s/@VERSION@/${version}/g" -e "s/@OBS@/${obs}/g" -e "s/@MACOS@/${macos}/g" <<'EOF'
## Installation

**Windows**
1. Download `replay-buffer-pro-@VERSION@-windows-x64.zip`.
2. Close OBS, extract the ZIP, and copy the `replay-buffer-pro` folder into `%ALLUSERSPROFILE%\obs-studio\plugins\` (typically `C:\ProgramData\obs-studio\plugins\`), overwriting the existing install.

**macOS**
1. Download `replay-buffer-pro-@VERSION@-macos-universal.pkg`.
2. Close OBS and run the installer. The plugin installs to `~/Library/Application Support/obs-studio/plugins/`.

## Requirements

- OBS Studio @OBS@+
- Windows 10/11 (64-bit) or macOS @MACOS@+

---

EOF
printf '%s\n' "$checksums"

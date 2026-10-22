#!/usr/bin/env bash
# Summarizes what a release cut from a develop commit would contain: the last
# tag, version and OBS dependency, every commit since that tag (including any
# that reached main without going through develop), the paths they touch
# outside the website and docs, and the pull requests and issues they cite.
#
# Usage: release-context.sh [commit]    (default: origin/develop)
set -euo pipefail

commit="${1:-origin/develop}"

git fetch --quiet --tags --prune origin
sha="$(git rev-parse --verify "${commit}^{commit}")"
if ! git merge-base --is-ancestor "$sha" origin/develop; then
  echo "error: ${commit} is not on origin/develop" >&2
  exit 1
fi

last_tag="$(git describe --tags --abbrev=0 --match '[0-9]*.[0-9]*.[0-9]*' "$sha")"
# The release ships everything on the commit plus anything already on main.
range=("$sha" origin/main "^${last_tag}")
spec() { git show "$1:buildspec.json" | jq -r "$2"; }

echo "# Release context"
echo "commit:            $(git log -1 --format='%h %s' "$sha")"
echo "last tag:          ${last_tag}"
echo "buildspec version: $(spec "$sha" .version) at commit (expected ${last_tag})"
echo "OBS dependency:    $(spec "$last_tag" '.dependencies["obs-studio"].version') at ${last_tag}, $(spec "$sha" '.dependencies["obs-studio"].version') at commit"
echo "on main only:      $(git rev-list --count --no-merges origin/main "^${sha}") commit(s) not in the release commit"
echo "today:             $(date +%F)"

echo
echo "## Commits since ${last_tag}"
git log --no-merges --format='%h %s (%an)' "${range[@]}"

echo
# Mirrors push.yaml's paths-ignore, plus pages/, where the website lived before site/.
echo "## Paths changed that push.yaml would build for"
plugin_paths="$(git log --format= --name-only "${range[@]}" | sort -u \
  | grep -vE '^(site/|docs/|pages/|\.claude/)|^\.github/workflows/(pages|site-check)\.yaml$|\.md$' || true)"
echo "${plugin_paths:-(none: website and docs only, so there is no plugin change to release)}"

# Pull requests and issues cited in commit messages, plus issues cited in
# those pull requests' bodies (suggestions, bug reports, "Closes #N").
refs="$(git log --format='%s%n%b' "${range[@]}" | grep -oE '#[0-9]+' | tr -d '#' | sort -un || true)"
for n in $refs; do
  if gh api "repos/{owner}/{repo}/issues/${n}" --jq '.pull_request != null' 2>/dev/null | grep -q true; then
    refs="${refs}"$'\n'"$(gh pr view "$n" --json body --jq .body | grep -oE '#[0-9]+' | tr -d '#' || true)"
  fi
done
refs="$(echo "$refs" | sort -un)"

echo
echo "## Pull requests and issues referenced"
for n in $refs; do
  gh api "repos/{owner}/{repo}/issues/${n}" 2>/dev/null --jq '
    "\(if .pull_request then "PR" else "issue" end) #\(.number) [\(.state)] \(.title)
  by \(.user.login) (\(.author_association)), \(.html_url)" +
    (if .pull_request then "\n\n" + ((.body // "") | gsub("\r"; "") | split("\n") | map("    " + .) | join("\n")) + "\n" else "" end)' \
    || echo "#${n}: not found"
done

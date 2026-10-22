---
name: release
description: Cut and ship a Replay Buffer Pro release end to end from a develop commit. It picks the semver version, bumps buildspec.json, adds the site changelog entry, opens the release/X.Y.Z pull request into main, waits for CI, merges, tags, back-merges main into develop, writes the GitHub release notes in the project's house style and publishes the draft release that CI builds. Use it whenever the user wants to release, ship, cut, tag or publish a new plugin version ("/release", "release 1.10.0", "ship what's on develop", "time for a patch release", "let's get 1.9.1 out"). Also use it when they ask for only one piece of that for this repo, such as release notes, a changelog entry or the release PR.
argument-hint: "[version] [develop-commit]"
---

# Release Replay Buffer Pro

Arguments: `$ARGUMENTS`. Either argument is optional: a version (`1.10.0`, which overrides the inferred one) and a commit on `origin/develop` (a SHA or ref, default the tip of `origin/develop`).

This skill ships a release the way 1.8.1 and 1.9.0 were shipped:

```
develop commit ─► release/X.Y.Z (version bump + changelog) ─► PR "Release X.Y.Z" into main ─► CI
  ─► [gate 1] ─► merge ─► tag X.Y.Z on the merge commit ─► CI builds a draft release
  ─► merge main back into develop ─► notes into the draft ─► [gate 2] ─► publish
```

If the user asked for only one piece, such as release notes or a changelog entry, do that piece with `references/writing-style.md` and stop.

## Why it works this way

- **CI builds everything.** Pushing a tag runs `.github/workflows/push.yaml`, which builds signed and notarized Windows and macOS packages and creates a **draft** release. The draft has the `.zip` and `.pkg` attached and a `### Checksums` body. Nothing is built locally, and you never run `gh release create`. Your job is to get the right commit onto `main`, tag it, and turn the draft into a finished release.
- **Tags are plain versions.** The tag is `X.Y.Z` with no `v` prefix, placed on `main`'s merge commit, because that commit is what users download. History uses lightweight tags. The workflow only releases tags matching `X.Y.Z` or `X.Y.Z-beta1`/`-rc1`, and ignores anything else.
- **Merging updates the website at once.** The site reads the version from `buildspec.json` and deploys from `main` when `buildspec.json` or `site/**` changes. The merge therefore publishes the new version number and changelog on the homepage within about a minute. Download buttons point at `/releases/latest`, so they keep serving the previous release until you publish. Keep the time between merge and publish short.
- **The back-merge matters for the next release.** It makes the tag reachable from `develop`, so `git describe` finds it next time, and it keeps `main` and `develop` from drifting apart.

## Ground rules

- **Each gate approves only the steps up to the next gate.** Before gate 1 you may create and push the release branch and open the PR. Those steps are easy to undo and don't touch `main`, `develop` or the website. Merging, tagging, pushing to `develop` and publishing need the user's yes at the matching gate. If something unexpected happens between gates, stop and report.
- **Leave the user's checkout alone.** They may be on another branch with uncommitted work. Do the release edits and the back-merge in temporary worktrees: run `mktemp -d` once, then `git worktree add` under that directory. Shell variables don't carry over between commands, so write the literal path wherever the steps below say `$tmp`. Remove the worktrees at the end.
- **Waits take minutes.** PR CI takes about 5 minutes, and the tag build takes 3 to 4. Run `gh … --watch` commands in the background, or with a 10-minute timeout, and continue when they finish. Don't poll in a loop.
- Run scripts from the repo root. They need `git`, `gh` (signed in, with push rights) and `jq`.

## 1. Gather context

```bash
.agents/skills/release/scripts/release-context.sh [commit]
```

The script prints the last tag, the version and OBS dependency at the commit, every commit since the tag, the paths that would trigger a plugin build, and each PR and issue the commits cite, with PR bodies and author associations (`OWNER`, `CONTRIBUTOR`, `NONE`) included. Read all of it, then check these before going on:

- **Only website, docs or CI workflow changes since the last tag** means there is nothing to release, because those reach users by merging `develop` into `main` (as in PR #54, "Publish new website pages"). Tell the user, offer that merge instead, and stop. The path list mirrors `push.yaml`'s `paths-ignore`, so check what is left in it: workflow edits and deleted old website files don't count, while `src/`, `data/`, `cmake/`, `buildspec.json` and packaging scripts do.
- **`buildspec version` differs from the last tag** means a previous back-merge is missing or a release stopped partway. See "Resuming" below.
- **`on main only` is above 0** means some commits reached `main` directly. They ship in this release and are already in the commit list. Mention them at gate 1.
- **The OBS dependency changed** means the minimum OBS version goes up. The notes need a `## ⚠️ Requirements changed` section (see the style guide).
- **The release branch, tag or release already exists** for the target version: see "Resuming".

## 2. Choose the version

Use the user's version if they gave one. Otherwise infer it from what users get:

- **Minor** (`1.9.0` → `1.10.0`): anything new a user can do or call, such as a feature, a new API field or a new setting. 1.8.0 and 1.9.0 were minor.
- **Patch** (`1.9.0` → `1.9.1`): only fixes, compatibility rebuilds or packaging changes. 1.8.1 and 1.5.1 were patches, and 1.5.1 raised the minimum OBS version.
- **Major:** never infer it. Suggest it at gate 1 only if the release removes or breaks part of the obs-websocket API or the saved settings, and let the user decide.

Judge from the changes themselves, not from commit prefixes, because squash-merged PRs often have plain titles. Check that `git ls-remote --tags origin X.Y.Z` is empty.

## 3. Write the texts

Read `references/writing-style.md` now. It defines the voice, format and examples for all three texts. Draft:

1. **The release notes** (opening sentence and sections, without the footer) into `$tmp/notes.md`. They go into the draft release at step 8, and the user sees them at gate 1.
2. **The changelog entry.**
3. **The release PR body.**

## 4. Cut the release branch and open the PR

```bash
git worktree add -b release/X.Y.Z "$tmp/release" <commit-sha>
```

In `$tmp/release`:

- Set `"version"` in `buildspec.json` to `X.Y.Z`. Change that one line only.
- Add the changelog entry to `site/src/content/changelog.md`.
- If this release adds something the usage page documents, note the version that introduced it in **both** `site/src/pages/usage.astro` and its twin `site/public/usage.md`. For example, 1.9.0 changed "needs Replay Buffer Pro 1.8.0 or later" to "…1.8.0 or later (1.9.0 for the `duration` field)". Skip this when no documented interface changed.

Commit everything as `chore: bump version to X.Y.Z`, then push and open the PR:

```bash
git -C "$tmp/release" push -u origin release/X.Y.Z
gh pr create --base main --head release/X.Y.Z --title "Release X.Y.Z" --body-file "$tmp/pr-body.md"
```

## 5. Wait for CI

```bash
gh pr checks <pr> --watch --fail-fast
```

If it says no checks are reported, wait a few seconds and run it again. Expect Build Project (macOS and Windows), plus Site Check because the changelog changed. If a check fails, stop and don't merge. Show the user the failing job (`gh run view <run-id> --log-failed | tail -80`). Fixes belong on `develop` through a normal PR, so let the user choose how to continue.

## 6. Gate 1: merge and tag

Show the user:

- The version, with a one-line reason ("minor: `SaveClip` gains a `duration` field").
- The PR link and its CI result.
- The changelog entry and the release notes draft, both in full.
- The issues the PR will close.
- Anything unusual: a requirements change, commits that were only on `main`, or a suggested major version.

Ask one question, "Merge PR #N into main and tag X.Y.Z?", using AskUserQuestion if it is available. If they want changes, edit `$tmp/notes.md`, or commit the fix to the release branch and push it (wait for CI again if code or the site changed), then ask again.

## 7. Merge, tag and back-merge

These steps run after the user says yes at gate 1. Do them in order and stop if one fails.

```bash
gh pr merge <pr> --merge                 # merge commit: "Merge pull request #N from …/release/X.Y.Z"
git push origin --delete release/X.Y.Z
git fetch origin --tags
sha="$(gh pr view <pr> --json mergeCommit --jq .mergeCommit.oid)"
git show "$sha:buildspec.json" | jq -r .version     # must print X.Y.Z
git tag X.Y.Z "$sha"
git push origin X.Y.Z
```

Pushing the tag starts the release build. Do the back-merge while it runs:

```bash
git worktree add --detach "$tmp/develop" origin/develop
git -C "$tmp/develop" merge --no-ff origin/main -m "Merge main into develop after X.Y.Z release"
git -C "$tmp/develop" push origin HEAD:develop
```

If the merge conflicts, run `git -C "$tmp/develop" merge --abort` and report the conflicting files. Leave the release itself to finish, because it no longer depends on `develop`.

## 8. Fill the draft release

Find the tag's build and wait for it to finish. It can take a few seconds to appear in the list.

```bash
gh run list --workflow push.yaml --branch X.Y.Z --limit 1 --json databaseId,status,url
gh run watch <run-id> --exit-status
```

When it succeeds, `gh release view X.Y.Z --json isDraft,assets` must show a draft with `replay-buffer-pro-X.Y.Z-macos-universal.pkg` and `replay-buffer-pro-X.Y.Z-windows-x64.zip`. Then put the finished body and the bare-version title on it, and leave it as a draft:

```bash
.agents/skills/release/scripts/compose-release-body.sh X.Y.Z "$tmp/notes.md" > "$tmp/body.md"
gh release edit X.Y.Z --title X.Y.Z --notes-file "$tmp/body.md"
```

If the build failed, look at `gh run view <run-id> --log-failed`. A runner or network failure deserves one `gh run rerun <run-id> --failed`. Anything else: stop and report. The tag stays in place, and deleting or moving it is the user's decision.

## 9. Gate 2: publish

Show the draft's URL, its two assets with sizes, and the full final body. Ask "Publish release X.Y.Z?". When they say yes:

```bash
gh release edit X.Y.Z --draft=false --latest
```

## 10. Check and clean up

- `gh api 'repos/{owner}/{repo}/releases/latest' --jq .tag_name` prints `X.Y.Z`.
- The Pages deploy that the merge started succeeded: `gh run list --workflow pages.yaml --branch main --limit 1`.
- Each issue listed under `Closes` is now closed.
- Remove the worktrees and the local branch: `git worktree remove "$tmp/release"`, `git worktree remove "$tmp/develop"`, `git branch -D release/X.Y.Z`.

Report the links to the release, the PR and the Pages run, the issues closed, and anything left open.

## Resuming

A release can stop partway because a gate was declined, CI failed or the session ended. Before redoing anything, find out how far it got and continue from the first step that is missing:

| Check | Command | If true, continue at |
|---|---|---|
| Release PR is open | `gh pr list --head release/X.Y.Z --state open` | 5 (worktree gone? `git worktree add "$tmp/release" release/X.Y.Z`) |
| PR merged, tag missing | `gh pr list --head release/X.Y.Z --state merged`, `git ls-remote --tags origin X.Y.Z` | 7, tag step |
| Tag pushed, `main` not in `develop` | `git merge-base --is-ancestor origin/main origin/develop` fails | 7, back-merge |
| Draft release exists | `gh release view X.Y.Z --json isDraft` | 8 (rewrite `notes.md` from the merged changelog entry if it's gone) |
| Release published | same | 10 |

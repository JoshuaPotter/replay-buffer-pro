# Release writing style

Each release needs three texts. Write them in this order, because each one is cut down from the one before:

1. **GitHub release notes**: the fullest text. Users read it when they update.
2. **Changelog entry** in `site/src/content/changelog.md`: the notes shortened for the website.
3. **Release PR body**: for the maintainer. It says what the release contains and how to check it.

All three use the website's copy voice. Write short declarative sentences in sentence case, with no superlatives, marketing or emoji. Lead with what changes for the user, then explain how it works. Give exact values rather than adjectives, such as "waits at most 2 seconds" or "from 1 second to 6 hours". Put API names, JSON, file names and log strings in backticks.

## What goes in

Work from the `release-context.sh` output, which lists the commits, pull request bodies and issues.

- **In:** anything a plugin user notices. That covers new capabilities, behavior changes, fixes, obs-websocket API changes, changed requirements, packaging changes (download size, install layout), and log lines people use to diagnose problems.
- **Out:** website, docs, README, CI-only changes, refactors, tests and agent config. The website deploys from `main` by itself, and release notes describe the plugin people download. One exception: a build or packaging change users can feel, such as a smaller download, goes under `## Tooling`.
- Write one bullet per user-visible effect, not one per commit or PR. Several commits for one feature become one bullet. One PR can become two bullets when it has two separate effects: 1.9.0's PR #59 became "Flexible durations" and "See how your duration was read".

## 1. GitHub release notes

The title is the bare version, `1.9.0`. Write the top part below to a scratch file. `scripts/compose-release-body.sh` adds the Installation, Requirements and Checksums sections, so never write those by hand.

```markdown
Replay Buffer Pro X.Y.Z <present-tense verb> <the main user-visible outcome>.

## Core
- **<Headline>** — <explanation>. <credit> (<link>)
```

### Opening sentence
Write one sentence that starts with "Replay Buffer Pro X.Y.Z" and states the headline change in a user's terms. If there are two themes, join them with "and".

- "Replay Buffer Pro 1.9.0 lets obs-websocket clients write clip durations the way their tools show them."
- "Replay Buffer Pro 1.8.1 fixes a rare obs-websocket deadlock."
- "Replay Buffer Pro 1.6.0 fixes replay trimming reliability and refreshes the dock UI to look more like a native OBS dock."

### Sections
Use `##` headings in this order, and include only the ones that have content:

- `## Core`: plugin behavior, meaning saving, trimming, hotkeys, the obs-websocket API and logs.
- `## Fixes`: replaces Core in a patch release that only fixes things (1.8.1).
- `## UI`: how the dock looks and what its controls do.
- `## Tooling`: packaging or build changes users notice.
- `## ⚠️ Requirements changed`: use this when the minimum OBS or OS version goes up. Give the old and new minimum, say that the build will not load on older versions, and link the last release that still works for people who can't update. 1.5.1 is the model. Other upgrade hazards, such as a moved install path, get the same treatment under their own heading (1.5.0: "## ⚠️ Updating on Windows - please read"). These warning headings are the only place emoji appear.

### Bullets
`- **<Headline>** — <explanation>. <credit> (<link>)`

- **Headline:** bold and in sentence case. It names the benefit, or the fixed symptom in the words a user would use. Examples: "No more freeze when restarting obs-websocket", "Clips end when you press save", "Save clips from other apps", "Flexible durations for `SaveClip`". Don't put a period inside the bold.
- **Em dash:** use a spaced em dash (` — `), then continue the sentence in lowercase unless the next word is a proper noun or code, as 1.8.0 to 1.9.0 do.
- **Explanation:** say what was wrong or missing, what happens now, and where the limits are. Give exact numbers and API shapes, and show a minimal example for an API (`{"durationSeconds": 120}`). When an API changes, say what stays compatible ("`durationSeconds` works exactly as before."). For long detail, link the usage guide section instead of listing everything: "See the [usage guide](https://joshuapotter.github.io/replay-buffer-pro/usage/#duration-formats) for every accepted form."
- **Credit:** put it after the explanation, before the link.
  - A PR from someone other than the maintainer (author association not `OWNER`): "Thanks @Lyelt!"
  - An issue someone else opened: "Suggested by MillerTKG in [#58](https://github.com/JoshuaPotter/replay-buffer-pro/issues/58)." or "Reported by … in …". Write the login without `@`.
  - The maintainer's own PRs and issues get no credit.
- **Link:** put it last, in parentheses, as a full URL. Use `/pull/N` for PRs and `/issues/N` for issues: `([#59](https://github.com/JoshuaPotter/replay-buffer-pro/pull/59))`. Link the PR that shipped the change. If an issue tracked the work and the PR adds nothing, link the issue instead (1.7.0 links #40). A second bullet from the same PR can leave out the link.

### Complete examples

Patch release (1.8.1):

```markdown
Replay Buffer Pro 1.8.1 fixes a rare obs-websocket deadlock.

## Fixes
- **No more freeze when restarting obs-websocket** — changing obs-websocket settings while a `SaveClip` request was in flight could hang OBS. The request now waits at most 2 seconds, then answers `{"accepted": false, "error": "timeout"}` and is dropped without saving anything. Thanks @Lyelt! ([#51](https://github.com/JoshuaPotter/replay-buffer-pro/pull/51))
```

Minor release (1.9.0):

```markdown
Replay Buffer Pro 1.9.0 lets obs-websocket clients write clip durations the way their tools show them.

## Core
- **Flexible durations for `SaveClip`** — a new `duration` field accepts seconds (`120` or `"120"`), clock times (`"5:22"`, `"1:05:00"`) or units (`"90s"`, `"1.5h"`, `"1h30m"`). Fractions round to the nearest second, so timer output like `"5:22.6"` can be sent as is. `durationSeconds` works exactly as before. See the [usage guide](https://joshuapotter.github.io/replay-buffer-pro/usage/#duration-formats) for every accepted form. Suggested by MillerTKG in [#58](https://github.com/JoshuaPotter/replay-buffer-pro/issues/58). ([#59](https://github.com/JoshuaPotter/replay-buffer-pro/pull/59))
- **See how your duration was read** — accepted responses now include `requestedSeconds`, so a client can check the parsed value even when the save is clamped to the buffer.
```

Use LF line endings. Some older releases have CRLF only because they were edited in the browser.

## 2. Changelog entry

Insert it at the top of `site/src/content/changelog.md`, below the intro paragraph and above the previous release. The site parses the `## X.Y.Z (YYYY-MM-DD)` heading, so keep that format exactly. The date is the day you cut the release, in local time (the `today` line of the context script).

```markdown
## 1.9.0 (2026-10-20)

Write clip durations the way your tools show them.

- **Flexible durations for `SaveClip`.** A new `duration` field accepts seconds (`120` or `"120"`), clock times (`"5:22"`, `"1:05:00"`) or units (`"90s"`, `"1.5h"`, `"1h30m"`). Fractions round to the nearest second, so timer output like `"5:22.6"` can be sent as is. `durationSeconds` works as before.
- **See how your duration was read.** Accepted responses include `requestedSeconds`, so a client can check the parsed value even when the save is clamped to the buffer.
```

Derive it from the release notes:

- **Summary line:** take the opening sentence, drop "Replay Buffer Pro X.Y.Z", and turn the rest into an imperative or a short noun phrase. "lets obs-websocket clients write clip durations the way their tools show them" becomes "Write clip durations the way your tools show them." "fixes a rare obs-websocket deadlock" becomes "A fix for a rare obs-websocket deadlock."
- **Bullets:** keep the same bullets in the same order and drop the section headings. End the headline with a period inside the bold, leave out the em dash, and start the explanation with a capital.
- **Drop** credits, PR and issue links, and pointers to the usage guide. Trim the explanation to what a user needs; 1.8.1's changelog says "answers `timeout`" where its notes show the full JSON.
- **Requirements change:** add a bullet such as "**New minimum OBS version: 32.2.0.** This build does not load on older OBS Studio versions. Stay on 1.5.0 if you are not ready to update OBS."

## 3. Release PR body

Title it `Release X.Y.Z` and base it on `main`. The body follows PR #60:

```markdown
## Summary

Releases X.Y.Z: <what it brings, in one line>. Closes #N.

## <Area, e.g. API, Fixes, Save correlation>

- <reviewer-facing bullet, terser and more technical than the notes> (#PR)

## Release

- `buildspec.json` version set to X.Y.Z
- Changelog entry for X.Y.Z in `site/src/content/changelog.md`
- <any other release-only edit, e.g. Usage page notes that the `duration` field needs X.Y.Z>

## Test plan

- [ ] CI builds pass for macOS and Windows
- [ ] After merge, tag `X.Y.Z` on `main` and confirm the draft release has both packages
- [ ] Pages deploy shows X.Y.Z on the homepage and the new changelog entry
```

- Add `Closes #N` for every **open** issue the release fixes. PRs merged into `develop` don't close issues, because `develop` isn't the default branch, so this PR into `main` is what closes them.
- In this body, a bare `#N` is enough, because GitHub links it.
- If the feature PRs had manual test steps that CI can't cover, repeat the important ones in the test plan (1.8.0 did).

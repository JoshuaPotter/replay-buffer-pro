# Flathub extension

Many Linux users run OBS Studio from Flathub, and Flatpak OBS can't load the `.deb`. It loads plugins that are Flatpak extensions of `com.obsproject.Studio`, so Replay Buffer Pro is packaged as one: `com.obsproject.Studio.Plugin.ReplayBufferPro`.

This folder holds the files submitted to Flathub and a Docker setup that builds the extension and runs it in Flatpak OBS on any machine, including Apple Silicon Macs.

| File | What it is |
|---|---|
| `com.obsproject.Studio.Plugin.ReplayBufferPro.yml` | The flatpak-builder manifest, with `@VERSION@` and `@SHA256@` placeholders |
| `com.obsproject.Studio.Plugin.ReplayBufferPro.metainfo.xml` | AppStream metadata for Flathub and software centres, with `@VERSION@` and `@DATE@` placeholders |
| `flathub.json` | Flathub build options: x86_64 only, as Flatpak OBS is |
| `run.sh`, `Dockerfile`, `session.sh`, `bwrap-no-seccomp` | The Docker build and test desktop (not submitted) |

## How it builds

GitHub releases are the only source. The manifest builds from a published release's `replay-buffer-pro-X.Y.Z-source.tar.xz`, the tarball CI's `package-ubuntu` makes with CPack and attaches to every tag's release, pinned by its sha256. It never builds from a git tag or branch.

- `runtime: com.obsproject.Studio` at `stable`, with `build-extension: true`. The extension builds against Flatpak OBS's own libobs, obs-frontend-api, Qt 6 and FFmpeg headers in `/app`, and the SDK's libcurl. At run time it uses the FFmpeg in `/app/lib` that OBS ships and the runtime's libcurl, the same libraries OBS itself loads.
- `sdk: org.freedesktop.Sdk//26.08` must match the freedesktop runtime Flatpak OBS is built on (26.08 since OBS 33). Check `flatpak info -m com.obsproject.Studio//stable` when OBS moves to a new one. The Flathub manifest of Aitum Multistream, another obs-plugintemplate extension this one is modelled on, still names 25.08.
- `prefix: /app/plugins/ReplayBufferPro`. Flatpak OBS mounts each extension under `/app/plugins/<name>` and merges their `lib/obs-plugins` and `share/obs/obs-plugins` folders, so the manifest sets `-DCMAKE_INSTALL_LIBDIR=lib` over the Ubuntu preset's `lib/x86_64-linux-gnu`.
- It configures with the `ubuntu-x86_64` preset, not `ubuntu-ci-x86_64`. A published tarball can't change, so a new warning from a newer GCC in a future SDK must not fail Flathub's rebuild of it.
- `x-checker-data` watches `releases/latest` on the GitHub API. When a release is published, Flathub's bot opens a pull request in the Flathub repo with the new tarball's URL and sha256, and adds a `<release>` entry to the metainfo there (`is-main-source`). Drafts and prereleases aren't `latest`, so it never sees them.

Because Flathub pins the sha256, never replace the source tarball of a published release. Fix a bad release with a new version.

## Build and test

Requires Docker. On Apple Silicon, turn on Docker Desktop's Rosetta emulation, as for the [Linux test desktop](../linux-desktop/README.md).

```bash
tools/flatpak/run.sh tarball   # build_flatpak/replay-buffer-pro-<version>-source.tar.xz from HEAD, as CI makes it
tools/flatpak/run.sh build     # build and install the extension from the newest tarball
tools/flatpak/run.sh up        # Flatpak OBS with the extension at http://localhost:6080
```

| Command | What it does |
|---|---|
| `run.sh tarball` | Exports `HEAD` with `git archive` (a clean tree, like CI's checkout), configures it with the `ubuntu-x86_64` preset in the Linux test desktop's image, and runs CPack's `package_source`. Commit first: uncommitted changes aren't in it. |
| `run.sh build [tarball]` | Fills in the manifest's placeholders, points its archive source at the tarball (default: the newest in `build_flatpak/` or `release/`), and runs `flatpak-builder --install` against `com.obsproject.Studio//stable`. The log is `build_flatpak/build.log`. Pass any tarball, such as one downloaded from a release. |
| `run.sh up` | Starts Flatpak OBS on a desktop in the browser, with obs-websocket on `localhost:4455`. Settings and logs are in `build_flatpak/config/`, recordings in `build_flatpak/recordings/`. A fresh config gets the Linux test desktop's starting settings. Closing OBS restarts it. |
| `run.sh manifest VERSION` | Writes the Flathub submission files for a published release to `build_flatpak/submission/`. |
| `run.sh lint` | Runs Flathub's linter on the submission files. |
| `run.sh shell`, `run.sh down`, `run.sh image` | A shell in the running desktop, stop it, rebuild the image. |

Flatpak, the freedesktop SDK and Flatpak OBS live in the Docker volume `rbp-flatpak`, about 1.5 GB on the first run (`docker volume rm rbp-flatpak` to reclaim it). The container runs privileged, because Flatpak sandboxes with bubblewrap. On an ARM host it runs bubblewrap through `bwrap-no-seccomp`, which drops its seccomp filter: an ARM kernel rejects the x86_64 filter. Flathub's own builders and x86_64 Linux hosts use bubblewrap as is.

To start the test, press **Start Replay Buffer**, open **Docks → Replay Buffer Pro**, wait a little, and press a save button. The log in `build_flatpak/config/logs/` shows `[ReplayBufferPro] Plugin loaded` and a `TRIM VERDICT: ok` line, and `<name>_trimmed.mkv` lands in `build_flatpak/recordings/`. The starting scene's FFmpeg test sources stay black here, because Flatpak OBS's FFmpeg has no `lavfi` input; OBS still records the canvas and its silent audio mix, so clips save and trim as usual. Add any other source to see moving video.

## Submit to Flathub

Do this once, after the first release that has a source tarball is published.

1. `tools/flatpak/run.sh manifest X.Y.Z` (the first release whose assets include `replay-buffer-pro-X.Y.Z-source.tar.xz`) writes the manifest, metainfo and `flathub.json` for that release to `build_flatpak/submission/`, with the release asset's URL, sha256 and date filled in.
2. `tools/flatpak/run.sh lint` runs Flathub's linter (`flatpak-builder-lint` from `org.flatpak.Builder`) on those files. Then `tools/flatpak/run.sh build <downloaded tarball>` and `run.sh up` to check that release in Flatpak OBS. Flathub's own test build on the pull request builds from the URL and lints the result again.
3. Fork [flathub/flathub](https://github.com/flathub/flathub) with "Copy the master branch only" unchecked, then branch from `new-pr`:

   ```bash
   git clone --branch new-pr git@github.com:<you>/flathub.git && cd flathub
   git checkout -b replay-buffer-pro
   cp ../replay-buffer-pro/build_flatpak/submission/* .
   git add . && git commit -m "Add com.obsproject.Studio.Plugin.ReplayBufferPro" && git push -u origin replay-buffer-pro
   ```

4. Open a pull request against `new-pr` (not `master`) titled "Add com.obsproject.Studio.Plugin.ReplayBufferPro", and answer the reviewers there. Comment `bot, build` to rerun the test build.

When it's merged, Flathub creates `flathub/com.obsproject.Studio.Plugin.ReplayBufferPro` and invites you as its maintainer. From then on that repository holds the live manifest and metainfo. Change this folder only to test a manifest change, and copy the change there.

## Each release

1. Publish the GitHub release as usual. Its source tarball is then final.
2. Within a few hours, Flathub's bot opens a pull request in `flathub/com.obsproject.Studio.Plugin.ReplayBufferPro` that moves the archive source to the new tarball and adds the release to the metainfo. Flathub builds it as a test build.
3. When the test build passes, merge the pull request. Flathub publishes the update to users within a few hours.

If OBS moves Flatpak OBS to a new freedesktop runtime, update `sdk:` in the Flathub repo's manifest (and here) to match.

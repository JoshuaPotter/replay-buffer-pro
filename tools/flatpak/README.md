# Flatpak bundle

Flatpak OBS, the OBS Studio build on Flathub, can't load the `.deb`. It loads plugins that are Flatpak extensions of `com.obsproject.Studio`, so Replay Buffer Pro is also packaged as one, `com.obsproject.Studio.Plugin.ReplayBufferPro`, and every GitHub release carries it as a single-file bundle: `replay-buffer-pro-X.Y.Z-x86_64.flatpak`.

Users install it next to Flatpak OBS and restart OBS:

```bash
flatpak install --user ./replay-buffer-pro-X.Y.Z-x86_64.flatpak
flatpak uninstall com.obsproject.Studio.Plugin.ReplayBufferPro   # to remove it
```

A bundle has no update source, so users install each new release's bundle the same way. A bundle is also built against the Flatpak OBS of its day: when Flatpak OBS moves to a new freedesktop runtime or FFmpeg, an installed bundle stops loading, and the fix is the latest release's bundle. A Flatpak OBS runtime change therefore needs a plugin release soon after. The same manifest is ready for Flathub, which would add automatic updates; see [Flathub, later](#flathub-later).

| File | What it is |
|---|---|
| `com.obsproject.Studio.Plugin.ReplayBufferPro.yml` | The flatpak-builder manifest, with `@SOURCE@` (the archive's `url:` or `path:`) and `@SHA256@` placeholders |
| `com.obsproject.Studio.Plugin.ReplayBufferPro.metainfo.xml` | AppStream metadata for software centres, with `@VERSION@` and `@DATE@` placeholders |
| `build-bundle.sh` | Builds the `.flatpak` from a source tarball. CI's `flatpak-build` job and `run.sh build` both run it |
| `fill-manifest.sh` | Fills the manifest's and metainfo's placeholders, with the release's tarball URL or a local tarball as the source. `build-bundle.sh` and `release-manifest.sh` both run it |
| `release-manifest.sh` | Writes the Flathub submission files for a published release, from its source tarball's URL, digest and date. `run.sh manifest` runs it |
| `flathub.json` | Flathub build options, for a later Flathub submission: x86_64 only, as Flatpak OBS is |
| `run.sh`, `Dockerfile`, `entrypoint.sh`, `session.sh`, `bwrap-no-seccomp` | The Docker build and test desktop. It shares the Linux test desktop's `start-display.sh` (X display and noVNC), `wait-for-desktop.sh` and starting OBS settings from `tools/linux-desktop/` |

## How it builds

The bundle is built from the release's own `replay-buffer-pro-X.Y.Z-source.tar.xz`, never from a git checkout. In CI, the Ubuntu job makes that tarball with CPack's `package_source`, and the `flatpak-build` job in `.github/workflows/build-project.yaml` runs `build-bundle.sh` on it in Flathub's build image (`ghcr.io/flathub-infra/flatpak-github-actions`, privileged, because Flatpak sandboxes builds with bubblewrap). On a tag, `push.yaml` attaches the `.flatpak` to the draft release with the other assets.

`build-bundle.sh` fills the manifest's placeholders with the local tarball as its archive source, runs `flatpak-builder` into an OSTree repo, and exports the extension with `flatpak build-bundle --runtime`.

- `runtime: com.obsproject.Studio` at `stable`, with `build-extension: true`. The extension builds against Flatpak OBS's own libobs, obs-frontend-api, Qt 6 and FFmpeg headers in `/app`, and the SDK's libcurl. At run time it uses the FFmpeg in `/app/lib` that OBS ships and the runtime's libcurl, the same libraries OBS itself loads.
- `sdk: org.freedesktop.Sdk//26.08` must match the freedesktop runtime Flatpak OBS is built on (26.08 since OBS 33), and the CI image's `freedesktop-26.08` tag must match it too. Check `flatpak info -m com.obsproject.Studio//stable` when OBS moves to a new one.
- `prefix: /app/plugins/ReplayBufferPro`. Flatpak OBS mounts each extension under `/app/plugins/<name>` and merges their `lib/obs-plugins` and `share/obs/obs-plugins` folders, so the manifest sets `-DCMAKE_INSTALL_LIBDIR=lib` over the Ubuntu preset's `lib/x86_64-linux-gnu`.
- It configures with the `ubuntu-x86_64` preset, not `ubuntu-ci-x86_64`, so a new warning from a newer GCC in a future SDK doesn't fail a release build.
- A per-user install (`--user`) works with a system-wide Flatpak OBS: Flatpak looks for an app's extensions in every installation.

## Build and test

Requires Docker. On Apple Silicon, turn on Docker Desktop's Rosetta emulation, as for the [Linux test desktop](../linux-desktop/README.md).

```bash
tools/flatpak/run.sh tarball   # build_flatpak/replay-buffer-pro-<version>-source.tar.xz from HEAD, as CI makes it
tools/flatpak/run.sh build     # build_flatpak/replay-buffer-pro-<version>-x86_64.flatpak from the latest tarball
tools/flatpak/run.sh up        # Flatpak OBS with that bundle installed, at http://localhost:6081
```

| Command | What it does |
|---|---|
| `run.sh tarball` | Exports `HEAD` with `git archive` (a clean tree, like CI's checkout), configures it with the `ubuntu-x86_64` preset in the Linux test desktop's image, and runs CPack's `package_source`. Commit first: uncommitted changes aren't in it. |
| `run.sh build [tarball]` | Runs `build-bundle.sh` on the tarball (default: the most recently made in `build_flatpak/` or `release/`; pass any, such as one downloaded from a release). The log is `build_flatpak/build.log`. |
| `run.sh up` | Starts Flatpak OBS on a desktop in the browser, after installing the newest `build_flatpak/*.flatpak` with `flatpak install --user` as a user would. obs-websocket is on `localhost:4456`. The ports differ from the Linux test desktop's, so both can run at once. Settings and logs are in `build_flatpak/config/`, recordings in `build_flatpak/recordings/`. A fresh config gets the Linux test desktop's starting settings. Closing OBS restarts it. |
| `run.sh manifest VERSION`, `run.sh lint` | Write and lint Flathub submission files for a published release (see below). |
| `run.sh shell`, `run.sh down`, `run.sh image` | A shell in the running desktop, stop it, rebuild the image. Run `image` after editing this folder or the Linux test desktop's `start-display.sh` or `obs-config/`, which the image copies; an existing image isn't rebuilt on its own. |

Flatpak, the freedesktop SDK and Flatpak OBS live in the Docker volume `rbp-flatpak`, about 1.5 GB on the first run (`docker volume rm rbp-flatpak` to reclaim it). The container runs privileged for bubblewrap. On an ARM host it runs bubblewrap through `bwrap-no-seccomp`, which drops its seccomp filter, because an ARM kernel rejects the x86_64 filter. CI and x86_64 Linux hosts use bubblewrap as is.

To test, press **Start Replay Buffer**, open **Docks → Replay Buffer Pro**, wait a little, and press a save button. The log in `build_flatpak/config/logs/` shows `[ReplayBufferPro] Plugin loaded` and a `TRIM VERDICT: ok` line, and `<name>_trimmed.mkv` lands in `build_flatpak/recordings/`. The starting scene's FFmpeg test sources stay black here, because Flatpak OBS's FFmpeg has no `lavfi` input; OBS still records the canvas and its silent audio mix, so clips save and trim as usual. Add any other source to see moving video.

## Flathub, later

The manifest, metainfo and `flathub.json` are what a Flathub submission needs. The manifest's archive source is a published release's source tarball URL pinned by sha256, and its `x-checker-data` watches `releases/latest` on the GitHub API, so once the extension is on Flathub, Flathub's bot opens an update pull request in `flathub/com.obsproject.Studio.Plugin.ReplayBufferPro` for each published release and adds the release to the metainfo there. From then on, never replace a published release's source tarball, because Flathub pins its sha256.

To submit:

1. `tools/flatpak/run.sh manifest X.Y.Z` writes the manifest, metainfo and `flathub.json` for a published release to `build_flatpak/submission/`, with its source tarball's URL, sha256 and date filled in.
2. `tools/flatpak/run.sh lint` runs Flathub's linter (`flatpak-builder-lint` from `org.flatpak.Builder`) on them.
3. Fork [flathub/flathub](https://github.com/flathub/flathub) with "Copy the master branch only" unchecked, branch from `new-pr`, copy in `build_flatpak/submission/*`, and open a pull request against `new-pr` titled "Add com.obsproject.Studio.Plugin.ReplayBufferPro".

After it's merged, each release needs only that bot pull request merged once its test build passes.

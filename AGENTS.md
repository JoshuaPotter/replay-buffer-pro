# AGENTS

This file is a concise handoff for agents working in the Replay Buffer Pro OBS plugin.

## Project summary
- Adds a dockable OBS UI for replay buffer controls.
- Lets users adjust replay buffer length and save clips of customizable durations.
- Trims saved replays to the last N seconds using FFmpeg libavformat (no re-encode).

## Architecture map (start here)
- Module entry + OBS integration: `src/main.cpp`
- Dock widget + UI orchestration: `src/plugin/plugin.hpp`, `src/plugin/plugin.cpp`
- obs-websocket `SaveClip` vendor request: `src/plugin/websocket-command.hpp`, `src/plugin/websocket-command.cpp`
- UI components: `src/ui/ui-components.hpp`, `src/ui/ui-components.cpp`
- Replay buffer manager: `src/managers/replay-buffer-manager.hpp`, `src/managers/replay-buffer-manager.cpp`
- Settings manager: `src/managers/settings-manager.hpp`, `src/managers/settings-manager.cpp`
- Save button settings: `src/managers/save-button-settings.hpp`, `src/managers/save-button-settings.cpp`
- Hotkey manager: `src/managers/hotkey-manager.hpp`, `src/managers/hotkey-manager.cpp`
- Utilities: `src/utils/obs-utils.*`, `src/utils/logger.hpp`, `src/utils/video-trimmer.*`, `src/utils/status-reporter.*`
- Config constants: `src/config/config.hpp`
- Localization: `data/locale/en-US.ini`
- Build system: `CMakeLists.txt`, `buildspec.json`, `CMakePresets.json`, `cmake/`
- CI/CD: `.github/workflows/`, `.github/actions/`, `.github/scripts/`
- Website (Astro, deployed to GitHub Pages): `site/`

## Core runtime flows
User-facing description of the interfaces below lives in `site/src/content/pages/usage.md`, which builds both the usage page and its `usage.md` twin. Update it when a flow's behavior, limits, or error strings change.

### Buffer length update
1. User steps or types a new value into the buffer length spinbox in the dock.
2. Debounce timer expires.
3. `SettingsManager::updateBufferLengthSettings(...)` writes `RecRBTime` into OBS profile config.
4. If a replay output exists, updates `max_time_sec` and calls `obs_output_update(...)`.

### Save segment
1. User clicks a duration button or hotkey.
2. `ReplayBufferManager::saveSegment(...)` validates buffer active and duration <= current length.
3. `requestSave(...)` refuses the save if OBS would drop it (buffer inactive, recording paused), otherwise arms it with `obs_frontend_replay_buffer_save()`, folds it into a not-yet-started outstanding request, or defers it behind a file being written.
4. On the replay buffer output's own `saved` signal (mux thread), the manager reads the path and posts it to `handleSaveCompleted(...)` on the Qt main thread, which queues a trim job and issues any deferred request.
5. The manager's worker thread trims to a `.rbp-partial.<ext>` file, verifies its duration, renames it to `_trimmed`, then deletes the original.

### Save clip over obs-websocket
1. A client sends `CallVendorRequest` with vendor `replay-buffer-pro`, request `SaveClip` and exactly one of `{"duration": ...}` or `{"durationSeconds": N}`.
2. The callback (obs-websocket thread) turns it into N seconds in 1..`MAX_BUFFER_LENGTH`. `durationSeconds` must be a whole JSON number. `duration` is a number or text (`"120"`, `"5:22.6"`, `"1:05:00"`, `"1h30m"`) parsed by `parseDurationSeconds(...)` in `src/utils/duration-parse.*`, and fractions round to the nearest second. The parser is pure C++ with unit tests in `tests/`, built with `-DENABLE_TESTS=ON`; keep the usage page in step with its accepted forms.
3. It posts the save to the Qt main thread and waits only until `saveSegment(...)` returns, never for file I/O. A null parent means no message boxes. N is clamped to the current buffer length, so a request longer than the buffer saves the whole buffer instead of failing (unlike the dock buttons, which refuse).
4. The response is `{"accepted": true, "durationSeconds": saved, "requestedSeconds": N, "clamped": bool}` or `{"accepted": false, "error": ...}` (`invalid-duration`, `buffer-inactive`, `save-refused`, `unavailable`, `timeout`).
5. `OBS_FRONTEND_EVENT_EXIT` and the dock destructor unregister the request and release any waiting callback, because OBS may stop servicing Qt events before it joins the WebSocket threads.
6. The wait is bounded to 2 seconds, because obs-websocket restarts its server on the main thread (settings dialog) and waits for in-flight requests. A timed-out request answers `timeout` and is abandoned, so it never saves later.

### Save full buffer
1. User clicks “Save Replay Buffer”.
2. `ReplayBufferManager::saveFullBuffer(...)` requests a `0`-duration save, an explicit do-not-trim marker.
3. The completion carrying that marker is logged `save-full-buffer` and left untrimmed.

### Correlating saves
- OBS holds a single armed save timestamp (`save_ts`) that a later request overwrites, so it never queues requests; a FIFO of requests drifts out of step with files. See `docs/architecture/replay-buffer-flow.md` for the OBS source citations.
- The manager keeps at most one outstanding request plus one deferred request (last-write-wins). A press before OBS starts writing folds into the outstanding request; a press while a file is being written is deferred; a press while a foreign save is being written waits behind a placeholder.
- A pre-flight gate mirrors OBS's own drop conditions (output inactive, video encoder paused), so a save OBS would drop is refused before any state exists.
- Completions come from the output's `saved` signal, not `OBS_FRONTEND_EVENT_REPLAY_BUFFER_SAVED`, which OBS suppresses when the buffer stopped mid-write or during scene collection/profile switches. Do not handle both, or every file is trimmed twice.
- The subscription is level-triggered via `ensureSubscribed()`, called on `REPLAY_BUFFER_STARTED`, every save request and every watchdog tick. It only acts while the buffer is active: OBS has no output handler at module load and the frontend API dereferences it unchecked, and an active buffer guarantees it exists. It keeps listening to an old output that still owes the outstanding request a file.
- A held request (deferred or waiting-foreign) is issued late, so its file ends after the key press. When it is issued, the manager records `heldNs`: the recorded time since the press, i.e. the wall-clock wait minus time recording was paused (`obs_encoder_get_pause_offset`), because paused time is not in the file. The trim job's `endOffsetSeconds` makes `VideoTrimmer::trimToWindow` end the clip at the press. Requests issued at once have exactly 0. A press older than the whole buffer fails with `window-not-in-buffer`; Save Replay Buffer saves are never trimmed back.
- A liveness-checked watchdog only releases requests OBS never started a file for. It never bounds how long a write takes (issue #40).
- The `saved` callback runs on the mux thread and must never block; `ensureSubscribed()` must disconnect before replacing the held output.
- A saved signal with nothing outstanding came from outside the plugin (OBS's own hotkey, tray, obs-websocket) and is logged `no-pending-request` but not trimmed.

## Key components and ownership
- `ReplayBufferPro::Plugin` (dock) owns UI, managers, timers, and OBS event wiring.
- `UIComponents` builds the UI and manages enabled/disabled state.
- `ReplayBufferManager` handles save requests and trimming, and owns the replay buffer output reference and its `saved` signal subscription.
- `SettingsManager` reads/writes OBS profile config and updates output settings.
- `HotkeyManager` registers per-duration hotkeys and persists bindings.
- `VideoTrimmer` trims using libavformat stream copy.

## Configuration and persistence
- Buffer length config key: `RecRBTime`.
- Config section is `AdvOut` for Advanced mode, otherwise `SimpleOutput`.
- Hotkey bindings are stored in `hotkey_bindings.json` under the module config path.
- Custom save button durations are stored in `save_button_settings.json` under the module config path.

## Build and localization
- Build system follows the [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate) pattern.
- Plugin metadata (name, version, author) and dependency versions are in `buildspec.json`.
- `CMakePresets.json` defines `windows-x64` and `macos` (universal) configure/build presets.
- Dependencies (OBS source, prebuilt obs-deps with FFmpeg, Qt6) are auto-downloaded into `.deps/` at configure time.
- OBS (libobs + obs-frontend-api) is built from source during configure.
- FFmpeg (avformat, avcodec, avutil) comes from the prebuilt obs-deps archive.
- CMake modules live in `cmake/common/` (cross-platform), `cmake/windows/` (MSVC-specific), and `cmake/macos/` (Xcode/macOS-specific).
- Windows DLL embeds VERSIONINFO via `cmake/windows/resources/resource.rc.in`.
- macOS builds produce a `.plugin` bundle; packaging uses `pkgbuild`/`productbuild` to produce a `.pkg` installer.
- Post-build rundir at `build_x64/rundir/<config>/` (Windows) or `build_macos/rundir/<config>/` (macOS) for quick testing.
- `prepare_release` custom CMake target creates a Windows zip package (Windows only; macOS packaging is handled by the CI `package-macos` script).
- GitHub Actions CI: builds on push/PR for Windows and macOS, creates draft releases on semver tag push. Pushes and PRs that only change the website (`site/**`, `pages.yaml`, `site-check.yaml`), Markdown files, `docs/**`, `.agents/**`, `.claude/**` or `skills-lock.json` skip the build (`paths-ignore` in `push.yaml` and `pr-pull.yaml`); tag pushes always build, because GitHub ignores path filters for tags.
- Releases: the `release` skill (`.agents/skills/release/`, `/release`) cuts one end to end from a develop commit: a `release/X.Y.Z` PR into `main`, a tag on the merge commit, a back-merge into `develop`, and notes on the draft release CI creates. Its `references/writing-style.md` is the house style for release notes and changelog entries.
- macOS CI uses Xcode's built-in compilation cache (CAS), not ccache — a ccache compiler-wrapper triggers a "conflicting deployment targets" error under Xcode 26.
- Locale strings in `data/locale/en-US.ini` accessed with `obs_module_text(...)`.
- C++ source code is fully cross-platform — no platform `#ifdef` guards required; all OS interactions go through OBS APIs and FFmpeg.

### Build commands (Windows)
```bash
cmake --preset windows-x64          # Configure (downloads deps on first run)
cmake --build --preset windows-x64  # Build
cmake --install build_x64 --config RelWithDebInfo  # Install
```

### Build commands (macOS)
```bash
cmake --preset macos                 # Configure (downloads deps on first run; requires Xcode 26.5+)
cmake --build --preset macos         # Build
cmake --install build_macos --config RelWithDebInfo  # Install to ~/Library/Application Support/obs-studio/plugins/
```

### Website build
`site/` is a separate Node project (Astro, Node 24 per `site/.nvmrc`). CMake, packaging and the plugin CI never read it; `.github/workflows/pages.yaml` builds and deploys it from `main`, and `site-check.yaml` builds it on pull requests.
```bash
cd site
npm ci              # Install pinned dependencies
npm run dev         # Dev server at http://localhost:4321/replay-buffer-pro/
npm run check       # Type-check .astro and .ts files
npm run build       # Static output in site/dist/
```

## Not present
- No custom OBS sources, filters, or outputs are registered. The only external API the plugin registers is the optional obs-websocket vendor request above; without obs-websocket it logs a warning and the dock still works. The plugin uses OBS frontend replay buffer APIs to save, and connects directly to the replay buffer output's `saved` signal and `get_last_replay` proc to learn when and where each file was written.

## Documentation upkeep
- More documentation is available in `docs/` and README.md.
- Project website source lives in `site/` and should be updated when relevant. Follow `.agents/rules/website-style.md` when adding or changing anything under it.
- The homepage is `site/src/pages/index.astro`, the changelog is `site/src/pages/changelog.astro` and the not-found page is `site/src/pages/404.astro`. The other subpages are Markdown files in `site/src/content/pages/`: `usage.md` (every way to save a clip: dock, hotkeys, obs-websocket; keep it the single detailed reference for these), `obs-replay-buffer-guide.md`, `compare.md`, `integrations.md` and `privacy.md` (website analytics and cookie consent; update it when the site adds or drops a tracking tool), rendered by `site/src/pages/[slug].astro`. Subpages use `SubpageLayout`; the shared head, header, closing CTA and footer live in `site/src/layouts/` and `site/src/components/`. The favicon and social image are in `site/public/`, rendered from `site/og/`. The hero's animated halftone flare is a WebGPU shader rendered with vgpu (`site/src/components/HeroField.astro`, `site/src/scripts/hero-field.ts` and `.wgsl`); the `vgpu` skill in `.agents/skills/` covers its API. Markup primitives (`Section`, `Surface`, `Card`, `DataTable`, `ExternalLink` and others) are in `site/src/components/ui/`, icons are SVG files in `site/src/icons/`, and shared URLs and nav links are constants in `site/src/data/site.ts`. Styles are scoped to their components, with `site/src/styles/global.css` for tokens and shared classes, and client scripts are bundled `<script>` tags in their components (shared code in `site/src/scripts/`). `.agents/rules/website-style.md` covers all of this.
- Each subpage's Markdown twin is built from its source, never hand-written: `site/src/pages/[slug].md.ts` serves `site/src/content/pages/<slug>.md` as `<slug>.md`, and `site/src/content/changelog.md` is the changelog's single source, rendered into the page and served as `changelog.md`. Add a `## x.y.z (YYYY-MM-DD)` section to it with each release.
- The homepage FAQ is the `faq` array in the frontmatter of `index.astro`, which renders both the visible `#faq` list and the `FAQPage` JSON-LD (answers with tags stripped). Edit it there only, and copy the wording into the FAQ in `site/src/content/llms.txt` by hand.
- A new page needs its Markdown file in `site/src/content/pages/`, a `PAGES` entry in `site/src/data/site.ts` (adds it to the footer, breadcrumbs and `sitemap.xml`), and a link in `site/src/content/llms.txt`.
- The build fills in the version from `buildspec.json` (homepage, `llms.txt`) and dates from git history (`sitemap.xml`, `llms.txt`). Don't hand-edit them; `llms.txt` keeps `{{RELEASE_DATE}}` and `{{VERSION}}` placeholders and CRLF line endings.
- See `.agents/rules/keep-docs-updated.md` for the rule on keeping `README.md`, `docs/`, `site/`, and this file in sync with project changes.
- Agent rules and skills live in `.agents/rules/` and `.agents/skills/` so any agent tool can read them. `.claude/rules` is a symlink to `.agents/rules` for Claude Code, and each skill in `.claude/skills/` is a symlink to its folder under `.agents/skills/` (Claude Code documents per-skill links, not a linked `skills` folder). Edit the files under `.agents/`, and add a `.claude/skills/<name>` link when you add a skill.

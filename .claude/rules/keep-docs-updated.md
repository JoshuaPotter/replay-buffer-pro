# Keep documentation updated

Before finishing any task that changes behavior, architecture, build steps, config keys, or file layout, check whether each file/folder below needs updating — and update it in the same change if so. Stale docs are a regression, not optional cleanup. If a change has no doc impact, that's fine — but check explicitly rather than skipping by default.

- `README.md` — project overview, installation instructions, usage examples, contribution guidelines (user-facing).
- `AGENTS.md` — architecture map, runtime flows, key components, build/config details (for LLM coding agents).
- `docs/*` — GitHub Pages website source (user-facing, public distribution).
- `reference/*` — developer-facing technical documentation and architecture details.

When in doubt about which file should change, prefer matching the audience: end users → `README.md`/`docs/*`; contributors/developers → `reference/*`; coding agents → `AGENTS.md`.

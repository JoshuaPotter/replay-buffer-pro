# Website style guide

Applies to everything under `pages/`. The homepage (`pages/index.html`) and `pages/styles.css` are the source of truth: a Geist-flavoured, Vercel-style monochrome dark system with flat black surfaces, hairline borders, no gradients or glows, and colour reserved for state. New pages must look like they belong to the homepage. Reuse what exists; add CSS only when no existing component fits.

## Tokens only
- Surfaces: `--bg`, `--bg-raised`, `--bg-subtle`. Text: `--text`, `--text-muted`, `--text-faint`. Lines: `--border`, `--border-strong`. Neutrals: `--gray-100…1000`.
- Radii: `--r-sm/md/lg/xl/full`. Cards, surfaces, and list panels use `--r-xl`; buttons use `--r-md`.
- Accent colours (`--blue`, `--blue-light`, `--green`, `--amber`, `--red`) mark state or links only (success, warning, inline links). Never decoration.
- No raw hex values, gradients, box-shadows, or glows in new CSS. (The only hex literals in use are `#000`/`#fff` for button text on a light primary button.)

## Type
- Geist and Geist Mono, loaded from the Google Fonts link already in the homepage `<head>`. Weights 400, 450, 500, 600.
- Headings are weight 500 with tight negative tracking: h1 `-0.055em`, h2 `-0.045em`, h3 `-0.02em`. Use `text-wrap: balance` on headings and `text-wrap: pretty` on lead and body copy.
- Inline code uses `<code>`; mono text elsewhere uses `--font-mono`.

## Layout
- Wrap page content in `.container` (max 1200px, 24px gutters, 16px under 30rem).
- Each block is a `.section` (vertical padding `clamp(3.5rem, 8vw, 6rem)`). Open it with `.section-intro` (max 44rem): an `h2` plus an optional muted lead paragraph.
- Breakpoints: `48rem` (two-column grids, footer grid), `60rem` (header nav appears, wider grids), `30rem` (tighter gutters, stacked rows). Design mobile first.

## Components to reuse
`.btn` with `.btn-primary` / `.btn-secondary` / `.btn-lg`, `.download-split`, `.surface` (+ `.surface-bar`), `.card` / `.card-lg` in `.card-grid` / `.card-grid-2` with `.card-eyebrow`, `.spec-list` / `.spec-row`, `.numbered-list`, `.check-list`, `.faq` (`details` / `summary`), `.issue-list` / `.issue`, `.tag`, `.log`, `.cta-panel`, `.site-header`, `.site-footer`.

When a new component is truly needed (for example long-form article text or a comparison table), add it to `pages/styles.css` in its own comment-banded section, built from the tokens above and the spacing and border language of `.spec-list` and `.issue-list`.

## Page shell
Every page, in this order:
1. `<!doctype html>`, `<html lang="en">`, charset and viewport metas, `color-scheme` `dark`, `theme-color` `#000`.
2. Unique `<title>` and meta description, canonical URL, Open Graph and Twitter tags, JSON-LD for the page type, the Google Fonts links, `styles.css`, and the Google Tag Manager snippet and noscript.
3. `.skip-link` to `#main`, then `.site-header`, then `<main id="main">`, then a closing `.section.section-cta` with `.cta-panel`, then `.site-footer` with the disclaimer.
4. Exactly one `<h1>`. Semantic landmarks. Visible focus states. Respect `prefers-reduced-motion`.

Pages are plain static HTML with no templating, so copy the header, footer, and closing CTA from `pages/index.html` verbatim (the footer's "Explore" column lists the content pages; add new ones there on the homepage first). On subpages, point section and page links relatively (`../#install`, `../compare/`) and load assets relatively (`../styles.css`, `../scripts/...`). Load `../scripts/index.js` as the homepage does; every call in it is a no-op when its element is missing, and it wires the download buttons, release version, and footer year.

## Copy voice
Short declarative sentences. Sentence-case headings. No emoji, no superlatives. Lead each section with a direct, self-contained answer; this is the text people and AI assistants quote.

## Checking a new page
Open it next to the homepage at desktop width and 375px. Confirm only tokens are used (grep new CSS and HTML for `#` colour literals), spacing and headings match, focus states and the skip link work, there is no horizontal scroll, and the console is clean.

# Website style guide

Applies to everything under `site/`. The homepage (`site/src/pages/index.astro`), the global stylesheet (`site/src/styles/global.css`) and the components' scoped styles are the source of truth: a Geist-flavoured, Vercel-style monochrome dark system with flat black surfaces, hairline borders, no gradients or glows, and colour reserved for state. New pages must look like they belong to the homepage. Reuse what exists; add CSS only when no existing component fits.

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
CSS classes: `.btn` with `.btn-primary` / `.btn-secondary` / `.btn-lg`, `.download-split`, `.surface` (+ `.surface-bar`), `.card` / `.card-lg` in `.card-grid` / `.card-grid-2` with `.card-eyebrow`, `.spec-list` / `.spec-row`, `.numbered-list`, `.check-list`, `.faq` (`details` / `summary`), `.issue-list` / `.issue`, `.tag`, `.log`, `.cta-panel`, `.site-header`, `.site-footer`.

Where an Astro component renders a pattern, use the component instead of writing its class markup by hand:
- Primitives in `site/src/components/ui/`: `Section` (`.section` + `.container`, with an optional `.section-intro` from `title` / `lead`), `Surface`, `Card`, `DataTable` (`.table-wrap` + `table.compare`; rows go in the slot), `ButtonLink` (`.btn` links), `ExternalLink` (adds `rel="noopener" target="_blank"`), `CopyCode` (`.click-to-copy`; the text goes in the slot so Windows backslashes stay literal), `Issue`, `JsonLd`, `Brand`.
- Composed components in `site/src/components/`: `SiteHeader`, `SiteFooter`, `ClosingCta`, `DownloadSplit`, `SocialMeta` (Open Graph, Twitter, author and colour-scheme meta), `CodeSample` (labelled `.log` panel; the page writes the `<code>`, so it can use `is:raw`), `FeatureRow`, `FactStrip`.
- Icons are SVG files in `site/src/icons/`, imported as components (`import Caret from '../icons/caret.svg'`). Don't paste SVG markup into pages.
- Repo, release, issue and discussion URLs, the screenshot URL, the author name and the homepage section links are constants in `site/src/data/site.ts`.
- `PluginDemo` is the homepage's interactive dock; its default buttons come from `site/src/data/demo.ts`.

Extract a new component when a pattern appears twice, or when it turns a long run of copied markup into a data array. Keep one-off markup in the page.

When a new component is truly needed, give it a scoped `<style>` built from the tokens above and the spacing and border language of `.spec-list` and `.issue-list`. Add CSS to `global.css` only for the cases listed under Astro rules.

## Page shell
The layouts render the shell, so pages hold only their own content:
- `BaseLayout` (`site/src/layouts/`) imports `site/src/styles/global.css` and renders the doctype, the shared head (charset, viewport, title, description, fonts, Google Tag Manager), the skip link, `SiteHeader`, `<main id="main">`, the closing `ClosingCta` and `SiteFooter` with the disclaimer. Page-specific head tags go in its `head` slot.
- `SubpageLayout` wraps `BaseLayout` for every page except the homepage. From its props it adds the Open Graph and Twitter tags, canonical URL, Markdown and `llms.txt` alternates, the `WebPage`/`TechArticle` and `BreadcrumbList` JSON-LD, and the `.page-head` (breadcrumb, the one `<h1>`, lead).
- Header, footer, closing CTA and the split download button are components in `site/src/components/`. Change them there, never by copying markup into a page.

Content subpages are Markdown files in the `pages` content collection (`site/src/content/pages/<slug>.md`, schema in `site/src/content.config.ts`). One file is both the page and its Markdown twin:
- `site/src/pages/[slug].astro` renders it at `/<slug>/` in `SubpageLayout`. The frontmatter holds the title, description, h1, lead, schema type and date, plus an optional `note` callout and `integrations` cards.
- `site/src/pages/[slug].md.ts` serves it as `/<slug>.md` (`site/src/lib/page-markdown.ts` writes the h1, lead, note and cards back out as Markdown).
- `site/src/lib/prose-plugin.ts`, a Sätteri hast plugin set in `astro.config.mjs`, gives Markdown the site's look: tables become `.table-wrap` > `table.compare` with the first cell of each row as its header, fenced code becomes a `.surface` code panel labelled by `title="…"` after the language, `https://joshuapotter.github.io/replay-buffer-pro/…` links become relative, and other links open in a new tab. Write site links as absolute URLs in the Markdown so the twin works on its own.
- `{#id}` after a heading sets its anchor (for example `{#obs-websocket}`). The twin drops it, and drops the fence `title="…"`.

To add a page:
1. Create `site/src/content/pages/<slug>.md` with the frontmatter above and the body in Markdown. A page whose layout Markdown can't express gets its own `site/src/pages/<slug>.astro` instead, as the homepage and changelog do.
2. Add a `PAGES` entry in `site/src/data/site.ts`. That adds it to the footer "Explore" column, the breadcrumb and `sitemap.xml`.
3. Add a link to the page and its `.md` twin in `site/src/content/llms.txt`.

## Astro rules
- Styles live with the markup they style, in a scoped `<style>` at the bottom of the component or page. `site/src/styles/global.css`, imported once by `BaseLayout`, holds only what can't be scoped: tokens, base element styles, shared layout and utility classes (`.container`, `.section`, `.btn`, `.cta-row`, `.card-grid`, `.tag`, `.mono`), and anything Markdown renders (`.prose`, `.table-wrap` / `.compare`, `.surface`, `.log`). Astro bundles, minifies and fingerprints both.
- A scoped style only reaches elements its own template renders. Wrap the rest in `:global()`: slot content and `set:html` output (`.card :global(p)`), child components and SVG icons (`.download-split-toggle :global(.caret)`), and elements a script creates (the demo's toasts use `<style is:global>`). Keep each media query with the rules it adjusts.
- A component that takes `class` spreads its other props onto the same element (`{...rest}`), which forwards the parent's `data-astro-cid-*` so the parent's scoped styles reach it. `Section`, `ButtonLink` and `ExternalLink` do this.
- Behaviour goes in a plain `<script>` (TypeScript, no `is:inline`) at the bottom of the component or page whose markup it drives: the mobile menu in `SiteHeader`, the split download menu and platform label in `DownloadSplit`, copy-on-click in `CopyCode`, the year and back-to-top in `SiteFooter`, the OS tabs and release version in `index.astro`, the dock demo in `PluginDemo` (logic in `site/src/scripts/plugin-demo.ts`). Astro bundles each once per page however many times the component renders, so a script queries every instance (`querySelectorAll('[data-…]')`) instead of assuming one.
- Code shared by scripts lives in `site/src/scripts/` (`os.ts`, `release.ts`). `detectPlatform()` returns `other` for phones, tablets, Linux and ChromeOS; `detectOS()` falls back to Windows for anything not macOS. `getLatestRelease()` fetches the GitHub release once per page for every caller. Client code can't import `site/src/data/site.ts`, which reads the repo with `node:fs` at build time.
- Third-party snippets (Google Tag Manager) and JSON-LD keep `is:inline`, so Astro emits them untouched. JSON-LD is the `JsonLd` component (`<script type="application/ld+json" is:inline set:html={…}>`).
- Literal `{` or `}` in page text (JSON examples in `<code>`) needs `is:raw` on the enclosing element, or Astro reads it as an expression.
- Links stay relative: the layouts take `root` (`''` on the homepage, `'../'` on subpages) and page bodies write `../` directly. Never start a link with `/`. Only the bundles Astro writes to `_astro/` use absolute `/replay-buffer-pro/` URLs, which come from `base` in `astro.config.mjs`.
- `compressHTML` stays `false` in `astro.config.mjs`. Astro 7's default strips whitespace between inline elements and changes rendered text.
- Close every non-void tag and never put a block element inside `<p>`; the compiler rejects or keeps invalid markup instead of correcting it.
- Astro drops top-level HTML comments in slotted content. Keep notes in the source; don't rely on them reaching the output.

## Copy voice
Short declarative sentences. Sentence-case headings. No emoji, no superlatives. Lead each section with a direct, self-contained answer; this is the text people and AI assistants quote.

## Checking a new page
Run `npm run check` and `npm run build` in `site/`, then open the page in the dev server (`npm run dev`, `http://localhost:4321/replay-buffer-pro/<slug>/` with the trailing slash) next to the homepage at desktop width and 375px. Confirm only tokens are used (grep new CSS and HTML for `#` colour literals), spacing and headings match, focus states and the skip link work, there is no horizontal scroll, and the console is clean.

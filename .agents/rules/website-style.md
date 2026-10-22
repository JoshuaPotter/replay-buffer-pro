# Website style guide

Applies to everything under `site/`. The homepage (`site/src/pages/index.astro`) and `site/public/styles.css` are the source of truth: a Geist-flavoured, Vercel-style monochrome dark system with flat black surfaces, hairline borders, no gradients or glows, and colour reserved for state. New pages must look like they belong to the homepage. Reuse what exists; add CSS only when no existing component fits.

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

When a new component is truly needed (for example long-form article text or a comparison table), add it to `site/public/styles.css` in its own comment-banded section, built from the tokens above and the spacing and border language of `.spec-list` and `.issue-list`.

## Page shell
The layouts render the shell, so pages hold only their own content:
- `BaseLayout` (`site/src/layouts/`) renders the doctype, the shared head (charset, viewport, title, description, fonts, `styles.css`, `scripts/index.js`, Google Tag Manager), the skip link, `SiteHeader`, `<main id="main">`, the closing `ClosingCta` and `SiteFooter` with the disclaimer. Page-specific head tags go in its `head` slot.
- `SubpageLayout` wraps `BaseLayout` for every page except the homepage. From its props it adds the Open Graph and Twitter tags, canonical URL, Markdown and `llms.txt` alternates, the `WebPage`/`TechArticle` and `BreadcrumbList` JSON-LD, and the `.page-head` (breadcrumb, the one `<h1>`, lead).
- Header, footer, closing CTA and the split download button are components in `site/src/components/`. Change them there, never by copying markup into a page.

To add a page:
1. Create `site/src/pages/<slug>.astro` using `SubpageLayout`, with the body as `.section` blocks.
2. Add a `PAGES` entry in `site/src/data/site.ts`. That adds it to the footer "Explore" column, the breadcrumb and `sitemap.xml`.
3. Add its Markdown twin at `site/public/<slug>.md` and a link to both in `site/src/content/llms.txt`.

## Astro rules
The build must emit the same plain HTML a hand-written page would:
- No `<style>` in `.astro` files and no CSS imports. Scoped styles add `data-astro-cid-*` attributes and bundle CSS; all CSS lives in `site/public/styles.css`.
- Client JS lives in `site/public/scripts/` and loads through the layout. Every `<script>` needs `is:inline`, or Astro bundles it into `_astro/`. JSON-LD is `<script type="application/ld+json" is:inline set:html={JSON.stringify(data, null, 2)}>`.
- Literal `{` or `}` in page text (JSON examples in `<code>`) needs `is:raw` on the enclosing element, or Astro reads it as an expression.
- Links and assets stay relative: the layouts take `root` (`''` on the homepage, `'../'` on subpages) and page bodies write `../` directly. Never start a link with `/`.
- `compressHTML` stays `false` in `astro.config.mjs`. Astro 7's default strips whitespace between inline elements and changes rendered text.
- Close every non-void tag and never put a block element inside `<p>`; the compiler rejects or keeps invalid markup instead of correcting it.
- Astro drops top-level HTML comments in slotted content. Keep notes in the source; don't rely on them reaching the output.

## Copy voice
Short declarative sentences. Sentence-case headings. No emoji, no superlatives. Lead each section with a direct, self-contained answer; this is the text people and AI assistants quote.

## Checking a new page
Run `npm run check` and `npm run build` in `site/`, then open the page in the dev server (`npm run dev`, `http://localhost:4321/replay-buffer-pro/<slug>/` with the trailing slash) next to the homepage at desktop width and 375px. Confirm only tokens are used (grep new CSS and HTML for `#` colour literals), spacing and headings match, focus states and the skip link work, there is no horizontal scroll, and the console is clean.

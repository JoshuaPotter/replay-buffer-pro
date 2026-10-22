import { defineCollection } from 'astro:content';
import { glob, type Loader } from 'astro/loaders';
import { z } from 'astro/zod';
import { readRepoFile } from './lib/repo';

const CHANGELOG = 'site/src/content/changelog.md';
const RELEASE_HEADING = /^## (\d+\.\d+\.\d+\S*) \((\d{4}-\d{2}-\d{2})\)$/;

/**
 * Splits changelog.md, the single source for the changelog, into one entry per
 * `## x.y.z (YYYY-MM-DD)` section. The page renders the entries; changelog.md is
 * served as written. Any other `## ` heading fails the build so a typo can't drop a release.
 */
const changelogLoader: Loader = {
  name: 'changelog',
  load: async ({ store, parseData, renderMarkdown, watcher }) => {
    const sync = async () => {
      store.clear();
      const sections = readRepoFile(CHANGELOG).split(/^(?=## )/m).slice(1);
      for (const [order, section] of sections.entries()) {
        const [heading, ...body] = section.split('\n');
        const match = RELEASE_HEADING.exec(heading);
        if (!match) throw new Error(`${CHANGELOG}: "${heading}" is not a "## x.y.z (YYYY-MM-DD)" release heading`);
        const [, version, date] = match;
        const data = await parseData({ id: version, data: { version, date, order } });
        store.set({ id: version, data, rendered: await renderMarkdown(body.join('\n')) });
      }
    };
    await sync();
    watcher?.on('change', (path) => {
      if (path.endsWith(CHANGELOG)) sync();
    });
  },
};

/**
 * Subpages written in Markdown. Each file renders at /<slug>/ through pages/[slug].astro and is served
 * as /<slug>.md through pages/[slug].md.ts, so the page and its Markdown twin share one source.
 */
const pages = defineCollection({
  loader: glob({ pattern: '*.md', base: './src/content/pages' }),
  schema: z.object({
    /** <title> and social title. */
    title: z.string(),
    description: z.string(),
    /** The page's h1, also the JSON-LD headline. */
    headline: z.string(),
    lead: z.string(),
    schemaType: z.enum(['WebPage', 'TechArticle']),
    datePublished: z.string(),
    /** A callout shown above the page body. */
    note: z.string().optional(),
    /** Cards shown between the note and the body (community integrations). */
    integrations: z
      .object({
        title: z.string(),
        lead: z.string(),
        items: z.array(
          z.object({
            eyebrow: z.string(),
            name: z.string(),
            /** Plain text with `code` spans. */
            description: z.string(),
            links: z.array(z.object({ label: z.string(), href: z.string() })),
          }),
        ),
      })
      .optional(),
  }),
});

export const collections = {
  pages,
  changelog: defineCollection({
    loader: changelogLoader,
    schema: z.object({ version: z.string(), date: z.string(), order: z.number() }),
  }),
};

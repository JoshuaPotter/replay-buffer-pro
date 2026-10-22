import { defineCollection } from 'astro:content';
import type { Loader } from 'astro/loaders';
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

export const collections = {
  changelog: defineCollection({
    loader: changelogLoader,
    schema: z.object({ version: z.string(), date: z.string(), order: z.number() }),
  }),
};

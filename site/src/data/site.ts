import { gitDate, readRepoFile } from '../lib/repo';

/** Absolute URL of the homepage, built from `site` and `base` in astro.config.mjs. */
export const SITE_URL = new URL(`${import.meta.env.BASE_URL.replace(/\/$/, '')}/`, import.meta.env.SITE).href;

/** Plugin version from buildspec.json, so the site always shows the version being released. */
export const VERSION: string = JSON.parse(readRepoFile('buildspec.json')).version;

/** Date of the last site or version change; dates the homepage and llms.txt. */
export const RELEASE_DATE = gitDate(['site', 'buildspec.json']);

export interface Page {
  slug: string;
  /** Footer "Explore" link, breadcrumb, and BreadcrumbList name. */
  label: string;
  changefreq: 'weekly' | 'monthly';
  priority: string;
  /** Repo-relative paths whose last commit dates the page in sitemap.xml. */
  sources: string[];
}

/** Content subpages, in footer and sitemap order. */
export const PAGES: Page[] = [
  { slug: 'usage', label: 'Usage', changefreq: 'monthly', priority: '0.8', sources: ['site/src/pages/usage.astro'] },
  { slug: 'obs-replay-buffer-guide', label: 'Replay buffer guide', changefreq: 'monthly', priority: '0.8', sources: ['site/src/pages/obs-replay-buffer-guide.astro'] },
  { slug: 'compare', label: 'Compare', changefreq: 'monthly', priority: '0.8', sources: ['site/src/pages/compare.astro'] },
  { slug: 'integrations', label: 'Community integrations', changefreq: 'monthly', priority: '0.6', sources: ['site/src/pages/integrations.astro'] },
  { slug: 'changelog', label: 'Changelog', changefreq: 'monthly', priority: '0.6', sources: ['site/src/pages/changelog.astro', 'site/src/content/changelog.md'] },
];

export function pageBySlug(slug: string): Page {
  const page = PAGES.find((p) => p.slug === slug);
  if (!page) throw new Error(`No PAGES entry for "${slug}" in src/data/site.ts`);
  return page;
}

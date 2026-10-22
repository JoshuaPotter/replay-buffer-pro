import type { APIRoute } from 'astro';
import { PAGES, RELEASE_DATE, SITE_URL } from '../data/site';
import { gitDate } from '../lib/repo';

const entries = [
  { loc: SITE_URL, lastmod: RELEASE_DATE, changefreq: 'weekly', priority: '1.0' },
  ...PAGES.map((page) => ({ loc: `${SITE_URL}${page.slug}/`, lastmod: gitDate(page.sources), changefreq: page.changefreq, priority: page.priority })),
  { loc: `${SITE_URL}llms.txt`, lastmod: RELEASE_DATE, changefreq: 'weekly', priority: '0.8' },
];

export const GET: APIRoute = () =>
  new Response(`<?xml version="1.0" encoding="UTF-8"?>
<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">
${entries
  .map(
    (e) => `  <url>
    <loc>${e.loc}</loc>
    <lastmod>${e.lastmod}</lastmod>
    <changefreq>${e.changefreq}</changefreq>
    <priority>${e.priority}</priority>
  </url>
`,
  )
  .join('')}</urlset>
`);

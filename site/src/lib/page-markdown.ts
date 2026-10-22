import type { CollectionEntry } from 'astro:content';

/**
 * The Markdown twin of a content page: the h1 and lead, the frontmatter note and integrations
 * written out as Markdown, then the body without the site-only syntax (`{#id}` heading anchors
 * and `title="…"` code fence labels).
 */
export function pageMarkdown({ data, body = '' }: CollectionEntry<'pages'>): string {
  const parts = [`# ${data.headline}`, data.lead];
  if (data.note) parts.push(`> ${data.note}`);
  if (data.integrations) {
    parts.push(`## ${data.integrations.title}`);
    for (const item of data.integrations.items) {
      parts.push(`### ${item.name}`, item.description, item.links.map(({ label, href }) => `[${label}](${href})`).join(' · '));
    }
  }
  const content = body
    .trim()
    .replace(/^(#{1,6} .*?) \{#[\w-]+\}$/gm, '$1')
    .replace(/^(```\w*) title="[^"]*"$/gm, '$1');
  return `${parts.join('\n\n')}\n\n${content}\n`;
}

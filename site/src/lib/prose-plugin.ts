import { defineHastPlugin } from 'satteri';
import type { Element, ElementContent } from 'hast';

/** Absolute site URLs in Markdown become relative links, so the source reads well as a standalone .md file. */
const SITE = 'https://joshuapotter.github.io/replay-buffer-pro/';

const isElement = (node: unknown, tag?: string): node is Element =>
  (node as Element)?.type === 'element' && (!tag || (node as Element).tagName === tag);

/**
 * Renders Markdown with the site's components and link rules: tables become .table-wrap > table.compare
 * with row headers, fenced code becomes a .surface code panel (labelled by the fence's title="…"), site links
 * become relative (every Markdown page sits one level below the root) and other links open in a new tab.
 */
export const prosePlugin = defineHastPlugin({
  name: 'replay-buffer-pro-prose',
  element: [
    {
      filter: ['a'],
      visit(node, ctx) {
        const href = String(node.properties.href ?? '');
        if (href.startsWith(SITE)) {
          ctx.setProperty(node, 'href', `../${href.slice(SITE.length)}`);
        } else if (/^https?:\/\//.test(href)) {
          ctx.setProperty(node, 'rel', 'noopener');
          ctx.setProperty(node, 'target', '_blank');
        }
      },
    },
    {
      filter: ['table'],
      visit(node, ctx) {
        ctx.setProperty(node, 'className', ['compare']);
        ctx.wrapNode(node, { raw: '<div class="table-wrap"></div>' });
      },
    },
    {
      filter: ['th'],
      visit(node, ctx) {
        ctx.setProperty(node, 'scope', 'col');
      },
    },
    {
      // The first cell of each body row labels the row.
      filter: ['td'],
      visit(node, ctx) {
        const row = ctx.parent(node);
        const index = ctx.indexOf(node) ?? 0;
        if (row.children.slice(0, index).some((child) => isElement(child))) return;
        ctx.replaceNode(node, { type: 'element', tagName: 'th', properties: { scope: 'row' }, children: node.children });
      },
    },
    {
      filter: ['pre'],
      visit(node, ctx) {
        const code = node.children.find((child) => isElement(child, 'code'));
        // Sätteri keeps the fence's info string after the language in data.meta.
        const meta = (code?.data as { meta?: string } | undefined)?.meta ?? '';
        const label = /title="([^"]+)"/.exec(meta)?.[1];
        const pre: Element = { ...node, properties: { className: ['log'] } };
        const children: ElementContent[] = [];
        if (label) {
          children.push({
            type: 'element',
            tagName: 'div',
            properties: { className: ['surface-bar'] },
            children: [{ type: 'element', tagName: 'span', properties: { className: ['surface-label', 'mono'] }, children: [{ type: 'text', value: label }] }],
          });
        }
        children.push({ type: 'element', tagName: 'div', properties: { className: ['surface-body'] }, children: [pre] });
        ctx.replaceNode(node, { type: 'element', tagName: 'div', properties: { className: ['surface'] }, children });
      },
    },
  ],
});

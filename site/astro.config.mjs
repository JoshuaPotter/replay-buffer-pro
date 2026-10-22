// @ts-check
import { defineConfig } from 'astro/config';
import { satteri } from '@astrojs/markdown-satteri';

export default defineConfig({
  site: 'https://joshuapotter.github.io',
  base: '/replay-buffer-pro',
  build: {
    // Subpages are emitted as <slug>/index.html; their ../ relative links depend on it.
    format: 'directory',
  },
  // Astro 7 defaults to 'jsx', which strips whitespace between inline elements
  // across newlines and changes rendered text. Keep markup exactly as written.
  compressHTML: false,
  markdown: {
    // The site uses straight quotes everywhere.
    processor: satteri({ features: { smartPunctuation: false } }),
    // Shiki writes inline colour styles; the site styles code with styles.css only.
    syntaxHighlight: false,
  },
});

// @ts-check
import { defineConfig } from 'astro/config';
import { satteri } from '@astrojs/markdown-satteri';
import { wgslVitePlugin } from '@vgpu/wgsl/loader-vite';
import { prosePlugin } from './src/lib/prose-plugin';

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
    processor: satteri({
      // The site uses straight quotes everywhere. `{#id}` after a heading sets its anchor.
      features: { smartPunctuation: false, headingAttributes: true },
      hastPlugins: [prosePlugin],
    }),
    // Shiki writes inline colour styles; the site styles code panels in global.css (.log).
    syntaxHighlight: false,
  },
  vite: {
    // Prepares imported .wgsl shaders at build time for vgpu (the hero's HeroField).
    plugins: [wgslVitePlugin()],
  },
});

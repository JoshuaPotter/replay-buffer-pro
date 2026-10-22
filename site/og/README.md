# Social image and touch icon sources

`public/og-image.png` (1200×630, the Open Graph and Twitter card image) and `public/apple-touch-icon.png` (180×180) are rendered from the HTML files here with headless Chrome. The touch icon is `public/favicon.svg` on black. Astro doesn't read this folder.

After editing a source, or `public/favicon.svg`, regenerate from `site/`:

```bash
"/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" --headless --hide-scrollbars --virtual-time-budget=5000 --window-size=1200,630 --screenshot="$PWD/public/og-image.png" "file://$PWD/og/og-image.html"
```

```bash
"/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" --headless --hide-scrollbars --window-size=180,180 --screenshot="$PWD/public/apple-touch-icon.png" "file://$PWD/og/apple-touch-icon.html"
```

On Windows or Linux, use that platform's Chrome or Chromium binary with the same flags. The images only change on the live site after a deploy from `main`.

---
title: "Privacy | Replay Buffer Pro"
description: "What the Replay Buffer Pro website measures, which cookies it sets, how consent works by region, and how to opt out."
headline: "Privacy"
lead: "This website measures visits with Google Analytics 4 and Microsoft Clarity, loaded through Google Tag Manager. The Replay Buffer Pro plugin itself collects nothing."
schemaType: "WebPage"
datePublished: "2026-10-22"
---

## What the plugin collects {#plugin}

Nothing. The Replay Buffer Pro plugin runs inside OBS Studio on your computer. It sends no analytics, telemetry or crash reports, and it makes no network requests of its own.

## What this website measures {#website}

The website loads Google Tag Manager, which loads Google Analytics 4 and Microsoft Clarity. Both run only when analytics are on, which depends on [where you are](https://joshuapotter.github.io/replay-buffer-pro/privacy/#consent).

- **Google Analytics 4** counts visits: the pages you view, how you reached the site (a search engine, a link or directly), and your browser, device and approximate location.
- **Microsoft Clarity** records how pages are used: clicks, scrolling, mouse movement and the pages you visit, plus your browser, device and approximate location. It uses this to build heatmaps and session recordings that show which parts of the site work and which confuse people.

These tools set cookies in your browser to recognise repeat visits. The data is processed by Google and Microsoft under their own policies:

- [Google Privacy Policy](https://policies.google.com/privacy)
- [Microsoft Privacy Statement](https://privacy.microsoft.com/privacystatement)

The site is hosted on GitHub Pages, and it loads fonts from Google Fonts and the latest release number from the GitHub API. Those services receive your IP address when your browser requests from them, as with any website.

## Consent by region {#consent}

| Where you are | Default | What you see |
| --- | --- | --- |
| EU, EEA, UK and Switzerland | Analytics off | A banner asking whether to allow analytics cookies |
| Everywhere else | Analytics on | A notice with an Opt out button |

Google Tag Manager decides your region from your IP address. If your browser sends a [Global Privacy Control](https://globalprivacycontrol.org/) signal, analytics stay off wherever you are.

## How to change your choice {#opt-out}

Use **Cookie settings** in the footer of any page to show the banner again and choose differently. Your choice is stored in your browser's local storage, so clearing site data resets it.

## Contact {#contact}

Questions about this page can go to the project's [GitHub issues](https://github.com/JoshuaPotter/replay-buffer-pro/issues).

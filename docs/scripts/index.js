/**
 * Main entry point for the Replay Buffer Pro landing page
 */
import { fetchLatestRelease, applyReleaseVersionToElements, applyDownloadUrls, updateYearByClass } from './utils.js';
import { initializePluginSlider } from './plugin-ui-demo.js';

// ES module scripts with defer are guaranteed to run after DOM parsing,
// so DOMContentLoaded wrapping is unnecessary and can cause missed events.
initializePluginSlider();

// Fetch the latest release once, then update the displayed version and point
// each platform's download buttons at the matching release asset.
const releaseVersion = document.querySelectorAll('.release-version');
const downloadLinks = document.querySelectorAll('[data-download]');
fetchLatestRelease()
  .then(release => {
    applyReleaseVersionToElements(releaseVersion, release.tag_name);
    applyDownloadUrls(downloadLinks, release.assets);
  })
  .catch(error => console.error('Release fetch failed', error));

// Update year in footer
const yearEls = document.querySelectorAll('.year');
updateYearByClass(yearEls);

// Scroll to top functionality
const scrollToTopLinks = document.querySelectorAll('.scroll-to-top');
scrollToTopLinks.forEach(link => {
  link.addEventListener('click', (e) => {
    e.preventDefault();
    window.scrollTo({
      top: 0,
      behavior: 'smooth'
    });
  });
});


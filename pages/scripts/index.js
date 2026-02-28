/**
 * Main entry point for the Replay Buffer Pro landing page
 */
import { fetchLatestReleaseTag, applyReleaseVersionToElements, updateYearByClass } from './utils.js';
import { initializePluginSlider } from './plugin-ui-demo.js';

// ES module scripts with defer are guaranteed to run after DOM parsing,
// so DOMContentLoaded wrapping is unnecessary and can cause missed events.
initializePluginSlider();

// Fetch and display latest release version
const releaseVersion = document.querySelectorAll('.release-version');
fetchLatestReleaseTag()
  .then(tag => applyReleaseVersionToElements(releaseVersion, tag))
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


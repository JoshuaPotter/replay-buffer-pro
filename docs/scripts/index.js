/**
 * Main entry point for the Replay Buffer Pro landing page
 */
import { fetchLatestReleaseTag, applyReleaseVersionToElements, updateYearByClass } from './utils.js';
import { initializePluginSlider } from './plugin-ui-demo.js';

document.addEventListener('DOMContentLoaded', () => {
  // Initialize plugin UI demo slider
  initializePluginSlider();

  // Fetch and display latest release version
  const releaseVersion = document.querySelectorAll('.release-version');
  fetchLatestReleaseTag()
    .then(tag => applyReleaseVersionToElements(releaseVersion, tag))
    .catch(error => console.error('Release fetch failed', error));

  // Update year in footer
  const yearEls = document.querySelectorAll('.year');
  updateYearByClass(yearEls);
});


import { fetchLatestReleaseTag, applyReleaseVersionToElements, updateYearByClass } from './utils.js';

document.addEventListener('DOMContentLoaded', () => {
  const releaseVersion = document.querySelectorAll('.release-version');
  fetchLatestReleaseTag()
    .then(tag => applyReleaseVersionToElements(releaseVersion, tag))
    .catch(error => console.error('Release fetch failed', error));

  const yearEls = document.querySelectorAll('.year');
  updateYearByClass(yearEls);
});



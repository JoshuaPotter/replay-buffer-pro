(function () {
  const releaseVersion = document.querySelectorAll('.release-version');

  async function fetchLatestRelease() {
    try {
      const res = await fetch('https://api.github.com/repos/JoshuaPotter/replay-buffer-pro/releases/latest', {
        headers: { 'Accept': 'application/vnd.github+json' },
      });
      if (!res.ok) throw new Error('Release fetch failed');
      const data = await res.json();
      releaseVersion.forEach(el => el.textContent = `${el.dataset.prefix ?? 'v'}${data.tag_name}`);
    } catch (error) {
      console.error('Release fetch failed', error);
    }
  }

  // Footer year
  const yearEl = document.getElementById('year');
  if (yearEl) yearEl.textContent = new Date().getFullYear();

  // Initialize
  fetchLatestRelease();
})();



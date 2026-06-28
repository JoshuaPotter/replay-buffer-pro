export async function fetchLatestRelease(owner = 'JoshuaPotter', repo = 'replay-buffer-pro') {
  const res = await fetch(`https://api.github.com/repos/${owner}/${repo}/releases/latest`, {
    headers: { 'Accept': 'application/vnd.github+json' },
  });
  if (!res.ok) throw new Error('Release fetch failed');
  return res.json();
}

export function applyReleaseVersionToElements(elements, tag) {
  if (!elements || !tag) return;
  elements.forEach(el => {
    const prefix = el.dataset.prefix ?? 'v';
    el.textContent = `${prefix}${tag}`;
  });
}

// Release asset names are versioned (e.g. replay-buffer-pro-1.5.0-windows-x64.zip),
// so download links can't be hardcoded. Match each download element's
// data-download platform to the matching asset and point its href at the
// real browser_download_url. Elements with no matching asset keep their
// existing href (the releases page) as a fallback.
const ASSET_MATCHERS = {
  windows: /windows.*\.zip$/i,
  macos: /macos.*\.pkg$/i,
};

export function applyDownloadUrls(elements, assets = []) {
  if (!elements) return;
  elements.forEach(el => {
    const matcher = ASSET_MATCHERS[el.dataset.download];
    if (!matcher) return;
    const asset = assets.find(a => matcher.test(a.name));
    if (asset) el.href = asset.browser_download_url;
  });
}

const OS_LABELS = {
  windows: 'Download for Windows (x64)',
  macos: 'Download for macOS',
};

// Best-effort detection of the visitor's desktop OS. There is no Linux build,
// so anything that isn't macOS falls back to Windows.
export function detectOS() {
  const platform = navigator.userAgentData?.platform || navigator.platform || '';
  const ua = navigator.userAgent || '';
  if (/mac/i.test(platform) || /Mac OS X|Macintosh/i.test(ua)) return 'macos';
  return 'windows';
}

// Point every [data-download-auto] element at the detected OS so its download
// link resolves to that platform's asset. Elements also marked
// [data-download-label] get their visible label set to match.
export function applyDetectedOS(elements, os = detectOS()) {
  if (!elements) return;
  elements.forEach(el => {
    el.dataset.download = os;
    if (el.hasAttribute('data-download-label')) {
      el.textContent = OS_LABELS[os] ?? 'Download';
    }
  });
}

export function updateYearByClass(yearEls = []) {
  yearEls.forEach(el => {
    el.textContent = new Date().getFullYear();
  });
}



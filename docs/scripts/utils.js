export async function fetchLatestReleaseTag(owner = 'JoshuaPotter', repo = 'replay-buffer-pro') {
  const res = await fetch(`https://api.github.com/repos/${owner}/${repo}/releases/latest`, {
    headers: { 'Accept': 'application/vnd.github+json' },
  });
  if (!res.ok) throw new Error('Release fetch failed');
  const data = await res.json();
  return data.tag_name;
}

export function applyReleaseVersionToElements(elements, tag) {
  if (!elements) return;
  elements.forEach(el => {
    const prefix = el.dataset.prefix ?? 'v';
    el.textContent = `${prefix}${tag}`;
  });
}

export function updateYearByClass(yearEls = []) {
  yearEls.forEach(el => {
    el.textContent = new Date().getFullYear();
  });
}



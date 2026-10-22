// Client code can't import data/site.ts, which reads the repo with node:fs at build time.
const LATEST_RELEASE_API = 'https://api.github.com/repos/JoshuaPotter/replay-buffer-pro/releases/latest';

export interface Release {
  tag_name: string;
  assets: { name: string; browser_download_url: string }[];
}

let latest: Promise<Release | null> | undefined;

/**
 * The latest GitHub release, fetched once per page however many components ask for it.
 * Resolves to null (and logs once) when the request fails, so callers keep their fallbacks.
 */
export function getLatestRelease(): Promise<Release | null> {
  latest ??= fetch(LATEST_RELEASE_API, {
    headers: { Accept: 'application/vnd.github+json' },
  })
    .then((res) => {
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      return res.json() as Promise<Release>;
    })
    .catch((error) => {
      console.error('Release fetch failed', error);
      return null;
    });
  return latest;
}

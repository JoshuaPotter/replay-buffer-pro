export type OS = 'windows' | 'macos';

// Best-effort detection of the visitor's desktop OS. There is no Linux build,
// so anything that isn't macOS falls back to Windows.
export function detectOS(): OS {
  const nav = navigator as Navigator & { userAgentData?: { platform?: string } };
  const platform = nav.userAgentData?.platform || navigator.platform || '';
  const ua = navigator.userAgent || '';
  if (/mac/i.test(platform) || /Mac OS X|Macintosh/i.test(ua)) return 'macos';
  return 'windows';
}

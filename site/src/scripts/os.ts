export type OS = 'windows' | 'macos';
export type Platform = OS | 'other';

// Best-effort detection of the visitor's platform. Phones, tablets, Linux and
// ChromeOS can't run a build, so they are 'other'. iPadOS reports itself as a
// Mac, so a Mac with a touch screen counts as 'other' too.
export function detectPlatform(): Platform {
  const nav = navigator as Navigator & { userAgentData?: { platform?: string; mobile?: boolean } };
  const platform = nav.userAgentData?.platform || navigator.platform || '';
  const ua = navigator.userAgent || '';
  if (nav.userAgentData?.mobile || /Android|iPhone|iPad|iPod|CrOS|Linux/i.test(`${platform} ${ua}`)) return 'other';
  if (/mac/i.test(platform) || /Mac OS X|Macintosh/i.test(ua)) return navigator.maxTouchPoints > 1 ? 'other' : 'macos';
  if (/win/i.test(platform) || /Windows/i.test(ua)) return 'windows';
  return 'other';
}

// The desktop OS to show first where one must be chosen, such as the install
// steps. Anything that isn't macOS falls back to Windows.
export function detectOS(): OS {
  return detectPlatform() === 'macos' ? 'macos' : 'windows';
}

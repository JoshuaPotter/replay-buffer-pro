/**
 * Every save button slot of the homepage dock demo, matching Config::SAVE_BUTTON_DEFAULTS
 * in the plugin. The first DEFAULT_BUTTON_COUNT are the default buttons; the rest are what
 * a button starts at when the properties modal raises the count.
 */
export const saveButtonSlots = [
  { seconds: 15, label: 'Last 15 Seconds' },
  { seconds: 30, label: 'Last 30 Seconds' },
  { seconds: 60, label: 'Last 1 Minute' },
  { seconds: 300, label: 'Last 5 Minutes' },
  { seconds: 900, label: 'Last 15 Minutes' },
  { seconds: 1800, label: 'Last 30 Minutes' },
  { seconds: 180, label: 'Last 3 Minutes' },
  { seconds: 600, label: 'Last 10 Minutes' },
  { seconds: 3600, label: 'Last 1 Hour' },
  { seconds: 7200, label: 'Last 2 Hours' },
  { seconds: 10800, label: 'Last 3 Hours' },
  { seconds: 21600, label: 'Last 6 Hours' },
];

/** Buttons a new install shows, Config::DEFAULT_SAVE_BUTTON_COUNT in the plugin */
export const DEFAULT_BUTTON_COUNT = 6;

/** The homepage dock demo's default save buttons */
export const clipButtons = saveButtonSlots.slice(0, DEFAULT_BUTTON_COUNT);

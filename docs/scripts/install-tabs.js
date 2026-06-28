/**
 * Windows/macOS tab toggle for the installation steps. Defaults to the
 * visitor's detected OS so they see the relevant steps without clicking.
 */
export function initOsTabs(container, defaultOs) {
  if (!container) return;
  const tabs = container.querySelectorAll('[data-os-tab]');
  const panels = container.querySelectorAll('[data-os-panel]');

  const select = (os) => {
    tabs.forEach(tab => {
      const selected = tab.dataset.osTab === os;
      tab.setAttribute('aria-selected', String(selected));
      tab.tabIndex = selected ? 0 : -1;
    });
    panels.forEach(panel => {
      panel.hidden = panel.dataset.osPanel !== os;
    });
  };

  tabs.forEach(tab => {
    tab.addEventListener('click', () => select(tab.dataset.osTab));
  });

  select(defaultOs);
}

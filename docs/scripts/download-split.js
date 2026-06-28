/**
 * Split download buttons: a primary action plus a caret that toggles a menu
 * of the other platform downloads. Enhances any [data-download-split] element.
 */
export function initDownloadSplits(splits = []) {
  splits.forEach(split => {
    const toggle = split.querySelector('.download-split-toggle');
    const menu = split.querySelector('.download-split-menu');
    if (!toggle || !menu) return;

    const close = () => {
      menu.hidden = true;
      toggle.setAttribute('aria-expanded', 'false');
    };
    const open = () => {
      menu.hidden = false;
      toggle.setAttribute('aria-expanded', 'true');
    };

    toggle.addEventListener('click', (e) => {
      e.stopPropagation();
      menu.hidden ? open() : close();
    });

    // Close after picking an option, when clicking elsewhere, or on Escape.
    menu.addEventListener('click', close);
    document.addEventListener('click', (e) => {
      if (!split.contains(e.target)) close();
    });
    document.addEventListener('keydown', (e) => {
      if (e.key === 'Escape') close();
    });
  });
}

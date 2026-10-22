/**
 * The homepage's interactive dock demo (components/PluginDemo.astro).
 */
import { clipButtons } from '../data/demo';

/** Default durations in seconds, matching Config::SAVE_BUTTONS in the real plugin */
const DEFAULT_DURATIONS = clipButtons.map(({ seconds }) => seconds);

/** Live duration state for the 6 clip buttons (mutated by the customize modal) */
let currentDurations = [...DEFAULT_DURATIONS];

/**
 * Formats a duration in seconds to a human-readable label.
 * Mirrors the C++ formatDurationLabel / formatDurationValue logic.
 * @param {number} seconds
 * @returns {string} e.g. "Last 15 Seconds", "Last 5 Minutes", "Last 1 Hour"
 */
function formatDurationLabel(seconds: number): string {
  if (seconds >= 3600 && seconds % 3600 === 0) {
    const hours = seconds / 3600;
    return `Last ${hours} ${hours === 1 ? 'Hour' : 'Hours'}`;
  }
  if (seconds >= 60 && seconds % 60 === 0) {
    const minutes = seconds / 60;
    return `Last ${minutes} ${minutes === 1 ? 'Minute' : 'Minutes'}`;
  }
  return `Last ${seconds} ${seconds === 1 ? 'Second' : 'Seconds'}`;
}

/**
 * Clamps a value to [1, 21600] and returns an integer.
 * Mirrors normalizeDurations in the real plugin.
 * @param {number} value
 * @returns {number}
 */
function clampDuration(value: string): number {
  const n = parseInt(value, 10);
  if (isNaN(n)) return DEFAULT_DURATIONS[0];
  return Math.max(1, Math.min(21600, n));
}

/**
 * Applies currentDurations to the 6 clip button labels.
 */
function applyDurationsToButtons() {
  document.querySelectorAll('.clip-btn').forEach((btn, i) => {
    if (i < currentDurations.length) {
      btn.textContent = formatDurationLabel(currentDurations[i]);
    }
  });
}

/**
 * Updates clip button enabled/disabled states based on buffer length.
 * Reads durations from currentDurations (not button text) for accuracy.
 * @param {number} bufferLengthSeconds
 */
function updateClipButtonStates(bufferLengthSeconds: number) {
  document.querySelectorAll<HTMLButtonElement>('.clip-btn').forEach((btn, i) => {
    const clipDuration = currentDurations[i] ?? 0;
    if (clipDuration > bufferLengthSeconds) {
      btn.disabled = true;
      btn.classList.add('disabled');
    } else {
      btn.disabled = false;
      btn.classList.remove('disabled');
    }
  });
}

/**
 * Initializes the Customize Save Buttons modal.
 * Opens on Customize button click, allows editing 6 durations in seconds,
 * saves with clamping, updates button labels, refreshes disabled states.
 */
function initializeCustomizeModal() {
  const overlay = document.getElementById('customize-modal');
  const openBtn = document.getElementById('customize-btn');
  const closeBtn = document.getElementById('modal-close');
  const cancelBtn = document.getElementById('modal-cancel');
  const saveBtn = document.getElementById('modal-save');
  const panel = document.getElementById('plugin-ui-demo');

  if (!overlay || !openBtn) return;
  const modal = overlay;

  const inputs = Array.from({ length: 6 }, (_, i) =>
    document.getElementById(`btn-input-${i}`) as HTMLInputElement | null
  );

  function openModal() {
    // Sync inputs to current durations before showing
    inputs.forEach((input, i) => {
      if (input) input.value = String(currentDurations[i] ?? DEFAULT_DURATIONS[i]);
    });
    modal.setAttribute('aria-hidden', 'false');

    // Recede the panel first, then reveal the modal after a beat
    // so the push-back is visible before focus shifts to the card.
    if (panel) panel.classList.add('panel-recessed');
    setTimeout(() => {
      modal.classList.add('show');
      if (inputs[0]) inputs[0].focus();
    }, 60);
  }

  function closeModal() {
    modal.classList.remove('show');
    modal.setAttribute('aria-hidden', 'true');
    if (panel) panel.classList.remove('panel-recessed');
    openBtn!.focus();
  }

  function saveModal() {
    const newDurations = inputs.map((input, i) =>
      input ? clampDuration(input.value) : DEFAULT_DURATIONS[i]
    );
    currentDurations = newDurations;

    // Reflect clamped values back into inputs
    inputs.forEach((input, i) => {
      if (input) input.value = String(currentDurations[i]);
    });

    applyDurationsToButtons();

    // Re-evaluate disabled states against the current buffer length value
    const bufferInput = document.getElementById('buffer-length-input') as HTMLInputElement | null;
    const bufferLength = bufferInput ? parseInt(bufferInput.value, 10) : 21600;
    updateClipButtonStates(bufferLength);

    closeModal();
  }

  openBtn.addEventListener('click', openModal);
  if (closeBtn) closeBtn.addEventListener('click', closeModal);
  if (cancelBtn) cancelBtn.addEventListener('click', closeModal);
  if (saveBtn) saveBtn.addEventListener('click', saveModal);

  // Close on backdrop click (click directly on overlay, not the modal card)
  modal.addEventListener('click', (e) => {
    if (e.target === modal) closeModal();
  });

  // Close on Escape key
  document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape' && modal.classList.contains('show')) {
      closeModal();
    }
  });

  // Allow Enter key to submit from any input
  inputs.forEach(input => {
    if (input) {
      input.addEventListener('keydown', (e) => {
        if (e.key === 'Enter') saveModal();
      });
    }
  });
}

/**
 * Initializes the plugin UI demo buffer length number-input interactions
 */
export function initPluginDemo() {
  const bufferInput = document.getElementById('buffer-length-input') as HTMLInputElement | null;
  const stepUpBtn = document.getElementById('buffer-step-up');
  const stepDownBtn = document.getElementById('buffer-step-down');

  if (!bufferInput) return;

  const input = bufferInput;
  const min = parseInt(input.min);
  const max = parseInt(input.max);

  function clampToRange(value: number): number {
    if (isNaN(value)) return min;
    return Math.max(min, Math.min(max, value));
  }

  function validateAndClampInput() {
    const value = clampToRange(parseInt(input.value));
    input.value = String(value);
    updateClipButtonStates(value);
  }

  function step(delta: number) {
    const value = clampToRange(parseInt(input.value) + delta);
    input.value = String(value);
    updateClipButtonStates(value);
  }

  // Apply initial button labels from currentDurations
  applyDurationsToButtons();

  // Update button states as the user types (without clamping mid-edit)
  input.addEventListener('input', () => {
    updateClipButtonStates(parseInt(input.value));
  });

  // Validate and clamp only when done editing
  input.addEventListener('blur', validateAndClampInput);

  // Wire the up/down step buttons
  if (stepUpBtn) stepUpBtn.addEventListener('click', () => step(1));
  if (stepDownBtn) stepDownBtn.addEventListener('click', () => step(-1));

  // Initialize button states based on the current buffer length
  updateClipButtonStates(parseInt(input.value));

  // Initialize customize modal
  initializeCustomizeModal();

  // Initialize easter egg
  initializeClipButtonEasterEgg();
}

/**
 * Easter egg: Clip button interactions with toast notifications and achievements
 */
function initializeClipButtonEasterEgg() {
const adjectives = ['excellent', 'impressive', 'unstoppable', 'dominating', 'monster', 'ultra', 'unreal', 'rampage'];
  const achievements = [
    { id: 'first', threshold: 1, title: 'FIRST CLIP', message: 'You have entered the arena.' },
    { id: 'curious', threshold: 5, title: 'CURIOUS ONE', message: 'Impressive. You hunger for more.' },
    { id: 'collector', threshold: 6, title: 'BUTTON COLLECTOR', message: 'You have tagged every button. Perfect.' },
    { id: 'enthusiast', threshold: 10, title: 'CONTENT CREATOR', message: 'Ten clips. Rampage.' },
    { id: 'addict', threshold: 25, title: 'CLIP ADDICT', message: 'Twenty-five clips. Unstoppable.' },
    { id: 'insane', threshold: 50, title: 'ARE YOU OKAY?', message: 'Fifty clicks? MONSTER KILL.' },
    { id: 'legend', threshold: 100, title: 'ABSOLUTE LEGEND', message: 'One hundred clips. GODLIKE.' }
  ];

  let clickCount = parseInt(localStorage.getItem('rbp-demo-clicks') || '0');
  let clickedButtons = new Set<number>(JSON.parse(localStorage.getItem('rbp-demo-buttons') || '[]'));
  let shownAchievements = new Set<string>(JSON.parse(localStorage.getItem('rbp-demo-achievements') || '[]'));

  // Create toast container
  let toastContainer = document.getElementById('toast-container');
  if (!toastContainer) {
    toastContainer = document.createElement('div');
    toastContainer.id = 'toast-container';
    toastContainer.className = 'toast-container';
    document.body.appendChild(toastContainer);
  }
  const toasts = toastContainer;

  function createToast(icon: string, title: string, subtitle: string, isAchievement = false) {
    const toast = document.createElement('div');
    toast.className = `clip-toast ${isAchievement ? 'achievement-toast' : ''}`;
    toast.innerHTML = `
      ${!isAchievement ? `<div class="toast-icon">${icon}</div>` : ''}
      <div class="toast-content">
        <strong class="toast-title">${title}</strong>
        ${subtitle ? `<div class="toast-subtitle">${subtitle}</div>` : ''}
      </div>
    `;
    
    toasts.appendChild(toast);
    
    // Trigger animation
    requestAnimationFrame(() => {
      toast.classList.add('show');
    });

    // Auto-remove after delay
    setTimeout(() => {
      toast.classList.add('fade-out');
      setTimeout(() => {
        toast.remove();
      }, 300);
    }, isAchievement ? 4000 : 3000);
  }

  function showClipToast(buttonText: string) {
    const timestamp = new Date().toISOString().replace(/[-:T]/g, '').slice(0, 14);
    const isReplayBuffer = buttonText.includes('Replay Buffer');
    
    let filename;
    let title;
    
    if (isReplayBuffer) {
      filename = `replay-buffer-${timestamp}.mp4`;
      title = 'Replay buffer saved!';
    } else {
      // Extract duration from button text (e.g., "Last 15 Seconds" -> "15s")
      const durationMatch = buttonText.match(/(\d+)\s+(Second|Minute|Hour)/i);
      let durationStr = '';
      
      if (durationMatch) {
        const value = durationMatch[1];
        const unit = durationMatch[2].toLowerCase();
        if (unit.startsWith('hour')) {
          durationStr = `${value}h`;
        } else if (unit.startsWith('minute')) {
          durationStr = `${value}m`;
        } else {
          durationStr = `${value}s`;
        }
      }
      
      const randomAdj = adjectives[Math.floor(Math.random() * adjectives.length)];
      filename = `${randomAdj}-clip-${durationStr}-${timestamp}.mp4`;
      title = 'Clip saved!';
    }
    
    const checkIcon = `<svg width="20" height="20" viewBox="0 0 24 24" fill="none">
      <path d="M9 12l2 2 4-4" stroke="currentColor" stroke-width="2" stroke-linecap="round"/>
      <circle cx="12" cy="12" r="10" stroke="currentColor" stroke-width="1.5"/>
    </svg>`;

    createToast(checkIcon, title, filename, false);
  }

  function checkAchievements() {
    achievements.forEach(achievement => {
      if (clickCount >= achievement.threshold && !shownAchievements.has(achievement.id)) {
        shownAchievements.add(achievement.id);
        localStorage.setItem('rbp-demo-achievements', JSON.stringify([...shownAchievements]));
        
        // Show achievement toast after a brief delay
        setTimeout(() => {
          createToast(achievement.title.split(' ')[0], achievement.title, achievement.message, true);
        }, 500);
      }
    });
  }

  // Add click handlers to all clip buttons
  document.querySelectorAll<HTMLButtonElement>('.clip-btn, .save-buffer-btn').forEach((btn, index) => {
    btn.addEventListener('click', () => {
      const buttonText = (btn.textContent ?? '').trim();

      // Button press animation
      btn.style.transform = 'scale(0.95)';
      setTimeout(() => btn.style.transform = '', 100);

      // Track stats
      clickCount++;
      clickedButtons.add(index);
      localStorage.setItem('rbp-demo-clicks', clickCount.toString());
      localStorage.setItem('rbp-demo-buttons', JSON.stringify([...clickedButtons]));

      // Check for button collector achievement
      if (clickedButtons.size === 6 && !shownAchievements.has('collector')) {
        clickCount = Math.max(clickCount, 6); // Ensure we trigger collector achievement
      }

      // Show clip saved toast
      showClipToast(buttonText);

      // Check for achievements
      checkAchievements();
    });
  });

  // Debug: Log current stats on load
  if (clickCount > 0) {
    console.log(`🎮 Easter Egg Stats: ${clickCount} clips saved, ${clickedButtons.size}/7 buttons clicked, ${shownAchievements.size} achievements unlocked`);
  }
}

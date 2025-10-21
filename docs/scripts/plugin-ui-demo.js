/**
 * Plugin UI Demo Component
 * Handles initialization of the interactive plugin demo
 */

/**
 * Initializes the plugin UI demo slider interactions
 */
export function initializePluginSlider() {
  const slider = document.getElementById('buffer-slider');
  const sliderValueInput = document.getElementById('slider-value-input');
  const sliderFill = document.getElementById('slider-fill');
  const sliderMarkers = document.querySelectorAll('.slider-marker');

  if (!slider || !sliderValueInput || !sliderFill) return;

  const min = parseInt(slider.min);
  const max = parseInt(slider.max);

  function updateFromSlider() {
    const value = parseInt(slider.value);
    
    // Update input value
    sliderValueInput.value = value;
    
    // Update fill width
    const percentage = ((value - min) / (max - min)) * 100;
    sliderFill.style.width = `${percentage}%`;
  }

  function updateFromInput() {
    let value = parseInt(sliderValueInput.value);
    
    // Only update if value is valid and within range
    if (!isNaN(value) && value >= min && value <= max) {
      slider.value = value;
      const percentage = ((value - min) / (max - min)) * 100;
      sliderFill.style.width = `${percentage}%`;
    }
  }

  function validateAndClampInput() {
    let value = parseInt(sliderValueInput.value);
    
    // Validate and clamp value
    if (isNaN(value)) {
      value = min;
    } else if (value < min) {
      value = min;
    } else if (value > max) {
      value = max;
    }
    
    // Update input and slider
    sliderValueInput.value = value;
    slider.value = value;
    
    // Update fill width
    const percentage = ((value - min) / (max - min)) * 100;
    sliderFill.style.width = `${percentage}%`;
  }

  // Initialize on load
  updateFromSlider();

  // Update on slider input
  slider.addEventListener('input', updateFromSlider);

  // Update slider as user types (without clamping)
  sliderValueInput.addEventListener('input', updateFromInput);
  
  // Validate and clamp only when done editing
  sliderValueInput.addEventListener('blur', validateAndClampInput);

  // Handle marker clicks
  sliderMarkers.forEach(marker => {
    marker.addEventListener('click', (e) => {
      const value = parseInt(e.target.dataset.value);
      slider.value = value;
      updateFromSlider();
      updateClipButtonStates(value);
    });
  });

  // Update button states on slider change
  slider.addEventListener('input', () => {
    updateClipButtonStates(parseInt(slider.value));
  });

  sliderValueInput.addEventListener('blur', () => {
    updateClipButtonStates(parseInt(sliderValueInput.value));
  });

  // Initialize button states based on current slider value
  updateClipButtonStates(parseInt(slider.value));

  // Initialize easter egg
  initializeClipButtonEasterEgg();
}

/**
 * Updates clip button enabled/disabled states based on buffer length
 */
function updateClipButtonStates(bufferLengthSeconds) {
  const clipButtons = document.querySelectorAll('.clip-btn');
  
  clipButtons.forEach(button => {
    const buttonText = button.textContent.trim();
    const durationMatch = buttonText.match(/(\d+)\s+(Second|Minute)/i);
    
    if (durationMatch) {
      const value = parseInt(durationMatch[1]);
      const unit = durationMatch[2].toLowerCase();
      const clipDurationSeconds = unit.startsWith('second') ? value : value * 60;
      
      // Disable if clip duration exceeds buffer length
      if (clipDurationSeconds > bufferLengthSeconds) {
        button.disabled = true;
        button.classList.add('disabled');
      } else {
        button.disabled = false;
        button.classList.remove('disabled');
      }
    }
  });
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
  let clickedButtons = new Set(JSON.parse(localStorage.getItem('rbp-demo-buttons') || '[]'));
  let shownAchievements = new Set(JSON.parse(localStorage.getItem('rbp-demo-achievements') || '[]'));

  // Create toast container
  let toastContainer = document.getElementById('toast-container');
  if (!toastContainer) {
    toastContainer = document.createElement('div');
    toastContainer.id = 'toast-container';
    toastContainer.className = 'toast-container';
    document.body.appendChild(toastContainer);
  }

  function createToast(icon, title, subtitle, isAchievement = false) {
    const toast = document.createElement('div');
    toast.className = `clip-toast ${isAchievement ? 'achievement-toast' : ''}`;
    toast.innerHTML = `
      ${!isAchievement ? `<div class="toast-icon">${icon}</div>` : ''}
      <div class="toast-content">
        <strong class="toast-title">${title}</strong>
        ${subtitle ? `<div class="toast-subtitle">${subtitle}</div>` : ''}
      </div>
    `;
    
    toastContainer.appendChild(toast);
    
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

  function showClipToast(buttonText) {
    const timestamp = new Date().toISOString().replace(/[-:T]/g, '').slice(0, 14);
    const isReplayBuffer = buttonText.includes('Replay Buffer');
    
    let filename;
    let title;
    
    if (isReplayBuffer) {
      filename = `replay-buffer-${timestamp}.mp4`;
      title = 'Replay buffer saved!';
    } else {
      // Extract duration from button text (e.g., "Last 15 Seconds" -> "15s")
      const durationMatch = buttonText.match(/(\d+)\s+(Second|Minute)/i);
      let durationStr = '';
      
      if (durationMatch) {
        const value = durationMatch[1];
        const unit = durationMatch[2].toLowerCase();
        durationStr = unit.startsWith('second') ? `${value}s` : `${value}m`;
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
  const clipButtons = document.querySelectorAll('.clip-btn, .save-buffer-btn');
  clipButtons.forEach((btn, index) => {
    btn.addEventListener('click', (e) => {
      const buttonText = e.target.textContent.trim();
      
      // Button press animation
      e.target.style.transform = 'scale(0.95)';
      setTimeout(() => e.target.style.transform = '', 100);

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


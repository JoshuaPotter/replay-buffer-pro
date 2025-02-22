/**
 * @file replay_buffer_pro.cpp
 * @brief Implementation of the Replay Buffer Pro plugin for OBS Studio
 * @author Joshua Potter
 * @copyright GPL v2 or later
 *
 * This file implements the ReplayBufferPro class functionality, providing
 * enhanced replay buffer controls for OBS Studio. The implementation handles:
 * - UI initialization and management
 * - OBS configuration integration
 * - Replay buffer state monitoring
 * - Segment saving functionality
 */

#include "replay_buffer_pro.hpp"

// Qt includes for UI implementation
#include <QVBoxLayout>
#include <QLabel>
#include <QHBoxLayout>
#include <QThread>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <stdexcept>

namespace {
	// Constants for buffer length configuration
	constexpr int MIN_BUFFER_LENGTH = 10;      // 10 seconds minimum
	constexpr int MAX_BUFFER_LENGTH = 3600;    // 1 hour maximum
	constexpr int DEFAULT_BUFFER_LENGTH = 300; // 5 minutes default

	/**
	 * @brief Structure defining save button configurations
	 */
	struct SaveButton {
		int duration;      ///< Duration in seconds
		const char* text;  ///< Button text translation key
	};

	/**
	 * @brief Array of predefined save button configurations
	 * 
	 * Defines the durations and labels for the quick-save buttons
	 */
	const SaveButton SAVE_BUTTONS[] = {
		{15, "Save15Sec"},   // 15 seconds
		{30, "Save30Sec"},   // 30 seconds
		{60, "Save1Min"},    // 1 minute
		{300, "Save5Min"},   // 5 minutes
		{600, "Save10Min"},  // 10 minutes
		{900, "Save15Min"},  // 15 minutes
		{1800, "Save30Min"}  // 30 minutes
	};
}

/**
 * @brief RAII wrapper for obs_data_t structures
 *
 * This class ensures that obs_data_t resources are properly released
 * when they go out of scope, preventing memory leaks.
 */
class OBSDataRAII {
private:
	obs_data_t* data;

public:
	explicit OBSDataRAII(obs_data_t* d) : data(d) {}
	~OBSDataRAII() { if (data) obs_data_release(data); }
	
	obs_data_t* get() const { return data; }
	bool isValid() const { return data != nullptr; }
	
	// Prevent copying to ensure single ownership
	OBSDataRAII(const OBSDataRAII&) = delete;
	OBSDataRAII& operator=(const OBSDataRAII&) = delete;
};

/**
 * @brief Default constructor for ReplayBufferPro
 * @param parent The parent widget
 * 
 * Initializes a new ReplayBufferPro widget with the specified parent.
 * Sets up the UI, loads initial buffer length, and connects signals/slots.
 */
ReplayBufferPro::ReplayBufferPro(QWidget *parent)
	: QDockWidget(parent)
	, slider(nullptr)
	, secondsLabel(nullptr)
	, saveFullBufferBtn(nullptr) {
	
	// Set window properties
	setWindowTitle(obs_module_text("ReplayBufferPro"));
	setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

	// Initialize components and state
	initializeUI();
	loadCurrentBufferLength();
	connectSignalsAndSlots();
	
	// Register for OBS events
	obs_frontend_add_event_callback(OBSFrontendEvent, this);
}

/**
 * @brief Main window constructor for ReplayBufferPro
 * @param mainWindow The OBS main window
 * 
 * Initializes a new ReplayBufferPro widget and docks it to the specified main window.
 * Automatically adds the widget to the right dock area.
 */
ReplayBufferPro::ReplayBufferPro(QMainWindow *mainWindow)
	: QDockWidget(mainWindow)
	, slider(nullptr)
	, secondsLabel(nullptr)
	, saveFullBufferBtn(nullptr) {
	
	// Set window properties
	setWindowTitle(obs_module_text("ReplayBufferPro"));
	setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

	// Initialize components and state
	initializeUI();
	loadCurrentBufferLength();
	connectSignalsAndSlots();
	
	// Register for OBS events
	obs_frontend_add_event_callback(OBSFrontendEvent, this);

	// Add to main window's right dock area
	mainWindow->addDockWidget(Qt::RightDockWidgetArea, this);
}

/**
 * @brief Initializes the user interface components
 * 
 * Creates and arranges all UI elements including:
 * - Buffer length slider and label
 * - Save buttons for different durations
 * - Full buffer save button
 * 
 * All elements are arranged in a vertical layout with appropriate sub-layouts.
 */
void ReplayBufferPro::initializeUI() {
	// Create main container and layout
	QWidget *container = new QWidget(this);
	QVBoxLayout *mainLayout = new QVBoxLayout(container);

	// Add buffer length label
	mainLayout->addWidget(new QLabel(obs_module_text("BufferLength"), container));

	// Create slider control group
	QHBoxLayout *sliderLayout = new QHBoxLayout();
	
	// Initialize slider with range
	slider = new QSlider(Qt::Horizontal, container);
	slider->setRange(MIN_BUFFER_LENGTH, MAX_BUFFER_LENGTH);
	
	// Initialize seconds display label
	secondsLabel = new QLabel(container);
	secondsLabel->setMinimumWidth(50); // Ensure enough space for text
	
	// Add slider components to layout
	sliderLayout->addWidget(slider);
	sliderLayout->addWidget(secondsLabel);
	mainLayout->addLayout(sliderLayout);

	// Create and add save buttons section
	QHBoxLayout *buttonLayout = new QHBoxLayout();
	initializeSaveButtons(buttonLayout);
	mainLayout->addLayout(buttonLayout);

	// Set the container as the dock widget's content
	setWidget(container);
}

/**
 * @brief Initializes the save duration buttons
 * @param layout The layout to add the buttons to
 * 
 * Creates buttons for each predefined duration in SAVE_BUTTONS array.
 * Each button is connected to saveReplaySegment with its specific duration.
 * Also creates the "Save Full" button for saving the entire buffer.
 */
void ReplayBufferPro::initializeSaveButtons(QHBoxLayout *layout) {
	// Clear any existing buttons
	saveButtons.clear();

	// Create buttons for each predefined duration
	for (const auto& btn : SAVE_BUTTONS) {
		// Create button with translated text
		auto button = new QPushButton(obs_module_text(btn.text), this);
		
		// Connect button click to save function with specific duration
		connect(button, &QPushButton::clicked, this, [this, duration = btn.duration]() {
			saveReplaySegment(duration);
		});
		
		// Add to layout and tracking vector
		layout->addWidget(button);
		saveButtons.push_back(button);
	}

	// Create and add the full buffer save button
	saveFullBufferBtn = new QPushButton(obs_module_text("SaveFull"), this);
	connect(saveFullBufferBtn, &QPushButton::clicked, this, &ReplayBufferPro::saveFullBuffer);
	layout->addWidget(saveFullBufferBtn);
}

/**
 * @brief Loads the current buffer length from OBS settings
 * 
 * Retrieves the replay buffer length from OBS configuration.
 * Handles both Simple and Advanced output modes.
 * Falls back to DEFAULT_BUFFER_LENGTH if:
 * - Config cannot be loaded
 * - Current value is 0 or invalid
 */
void ReplayBufferPro::loadCurrentBufferLength() {
	// Try to get OBS configuration
	config_t* config = obs_frontend_get_profile_config();
	if (!config) {
		blog(LOG_ERROR, "Failed to get OBS profile config");
		setBufferLength(DEFAULT_BUFFER_LENGTH);
		return;
	}

	// Determine output mode and section
	const char* mode = config_get_string(config, "Output", "Mode");
	const char* section = (mode && strcmp(mode, "Advanced") == 0) ? "AdvOut" : "SimpleOutput";
	
	// Get current replay time and convert from milliseconds to seconds
	uint64_t replayTime = config_get_uint(config, section, "RecRBTime");
	int currentLength = static_cast<int>(replayTime / 1000);
	
	// Set length, defaulting if current value is invalid
	setBufferLength(currentLength > 0 ? currentLength : DEFAULT_BUFFER_LENGTH);
}

/**
 * @brief Updates the buffer length in the UI
 * @param seconds New buffer length in seconds
 * 
 * Updates:
 * - Slider position
 * - Seconds label text
 * - Save button states based on new length
 */
void ReplayBufferPro::setBufferLength(int seconds) {
	// Update UI components
	slider->setValue(seconds);
	secondsLabel->setText(QString::number(seconds) + "s");
	
	// Update button states based on new length
	updateButtonStates(seconds);
}

/**
 * @brief Updates the enabled state of save buttons
 * @param bufferLength Current buffer length in seconds
 * 
 * Enables/disables save buttons based on whether their duration
 * is less than or equal to the current buffer length.
 */
void ReplayBufferPro::updateButtonStates(int bufferLength) {
	for (size_t i = 0; i < saveButtons.size(); i++) {
		saveButtons[i]->setEnabled(bufferLength >= SAVE_BUTTONS[i].duration);
	}
}

/**
 * @brief Connects all signal/slot relationships
 * 
 * Sets up the following connections:
 * - Slider value changes -> Buffer length updates
 * - Buffer length updates -> OBS settings updates
 */
void ReplayBufferPro::connectSignalsAndSlots() {
	// Connect slider value changes to update handlers
	connect(slider, &QSlider::valueChanged, this, [this](int value) {
		setBufferLength(value);
		updateReplayBufferSettings(value);
	});
}

/**
 * @brief Updates the replay buffer settings in OBS
 * @param seconds New buffer length in seconds
 * 
 * Updates both the OBS configuration and output settings.
 * If the replay buffer is active:
 * 1. Stops the buffer
 * 2. Updates settings
 * 3. Restarts the buffer
 * 
 * @throws std::runtime_error if OBS configuration cannot be accessed
 */
void ReplayBufferPro::updateReplayBufferSettings(int seconds) {
	try {
		// Get OBS configuration
		config_t* config = obs_frontend_get_profile_config();
		if (!config) {
			throw std::runtime_error("Failed to get OBS profile config");
		}

		// Determine output mode (Simple/Advanced) and corresponding section
		const char* mode = config_get_string(config, "Output", "Mode");
		const char* section = (mode && strcmp(mode, "Advanced") == 0) ? "AdvOut" : "SimpleOutput";
		
		// Convert seconds to milliseconds for OBS config

		// Skip update if value hasn't changed
		if (config_get_uint(config, section, "RecRBTime") == seconds) {
			return;
		}

		// Handle active replay buffer
		bool wasActive = obs_frontend_replay_buffer_active();
		if (wasActive) {
			// Must stop buffer before changing settings
			obs_frontend_replay_buffer_stop();
			QThread::msleep(500); // Give OBS time to fully stop the buffer
		}

		// Update OBS configuration
		config_set_uint(config, section, "RecRBTime", seconds);
		config_save(config);

		// Update replay buffer output settings
		// if (obs_output_t* replay_output = obs_frontend_get_replay_buffer_output()) {
		// 	if (obs_data_t* settings = obs_output_get_settings(replay_output)) {
		// 		// Set max time in seconds for the output
		// 		obs_data_set_int(settings, "max_time_sec", seconds);
		// 		obs_output_update(replay_output, settings);
		// 		obs_data_release(settings);
		// 	}
		// 	obs_output_release(replay_output);
		// }

		// Save all OBS settings
		obs_frontend_save();

		// Restart buffer if it was active
		if (wasActive) {
			QThread::msleep(500); // Give OBS time before restarting
			obs_frontend_replay_buffer_start();
		}

	} catch (const std::exception& e) {
		// Log error and show user-friendly message
		blog(LOG_ERROR, "Failed to update buffer length: %s", e.what());
		QMessageBox::warning(this, obs_module_text("Error"), 
			QString(obs_module_text("FailedToUpdateLength")).arg(e.what()));
	}
}

/**
 * @brief Saves a segment of the replay buffer
 * @param duration Duration in seconds to save
 * 
 * Verifies:
 * 1. Replay buffer is active
 * 2. Requested duration doesn't exceed buffer length
 * 
 * Shows appropriate warning messages if conditions aren't met.
 */
void ReplayBufferPro::saveReplaySegment(int duration) {
	// First check if replay buffer is running
	if (!obs_frontend_replay_buffer_active()) {
		QMessageBox::warning(this, obs_module_text("Warning"), 
			obs_module_text("ReplayBufferNotActive"));
		return;
	}

	// Get current buffer configuration
	config_t* config = obs_frontend_get_profile_config();
	if (!config) return;

	// Determine output mode and section
	const char* mode = config_get_string(config, "Output", "Mode");
	const char* section = (mode && strcmp(mode, "Advanced") == 0) ? "AdvOut" : "SimpleOutput";
	
	// Get current buffer length (stored in milliseconds, convert to seconds)
	uint64_t currentBufferTime = config_get_uint(config, section, "ReplayTime") / 1000;

	// Verify requested duration is valid
	if (duration > static_cast<int>(currentBufferTime)) {
		// Show warning if duration exceeds buffer length
		QMessageBox::warning(this, obs_module_text("Warning"),
			QString(obs_module_text("CannotSaveSegment"))
				.arg(duration)
				.arg(currentBufferTime));
		return;
	}

	// Trigger the save operation
	obs_frontend_replay_buffer_save();
	blog(LOG_INFO, "Saving replay segment of %d seconds", duration);
}

/**
 * @brief Saves the entire replay buffer
 * 
 * Simple wrapper around obs_frontend_replay_buffer_save() that adds
 * error checking and user feedback.
 */
void ReplayBufferPro::saveFullBuffer() {
	// Simple save with active check
	if (obs_frontend_replay_buffer_active()) {
		obs_frontend_replay_buffer_save();
	} else {
		QMessageBox::warning(this, obs_module_text("Error"), 
			obs_module_text("ReplayBufferNotActive"));
	}
}

/**
 * @brief Static callback for OBS frontend events
 * @param event The event type from OBS
 * @param ptr Pointer to the ReplayBufferPro instance
 * 
 * Handles replay buffer state changes by updating the UI accordingly.
 * Uses QMetaObject::invokeMethod to ensure UI updates happen on the main thread.
 */
void ReplayBufferPro::OBSFrontendEvent(enum obs_frontend_event event, void *ptr) {
	// Cast the void pointer back to our class instance
	auto window = static_cast<ReplayBufferPro*>(ptr);
	
	// Handle replay buffer state changes
	switch (event) {
		case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTING:
		case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPING:
		case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED:
		case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED:
			// Use Qt's event system to safely update UI from any thread
			QMetaObject::invokeMethod(window, "updateSliderState", Qt::QueuedConnection);
			break;
		default:
			break;
	}
}

/**
 * @brief Updates the UI state based on replay buffer activity
 * 
 * - Disables slider when buffer is active
 * - Updates current buffer length when active
 */
void ReplayBufferPro::updateSliderState() {
	// Check current buffer state
	bool isActive = obs_frontend_replay_buffer_active();
	
	// Disable slider while buffer is active to prevent changes
	slider->setEnabled(!isActive);
	
	// Update current length if buffer is active
	if (isActive) {
		loadCurrentBufferLength();
	}
}

/**
 * @brief Destructor
 * 
 * Removes the OBS frontend event callback to prevent callbacks
 * after object destruction.
 */
ReplayBufferPro::~ReplayBufferPro() {
	// Clean up event callback to prevent dangling pointer
	obs_frontend_remove_event_callback(OBSFrontendEvent, this);
}
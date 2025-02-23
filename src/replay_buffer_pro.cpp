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
#include <QGridLayout>
#include <QSizePolicy>
#include <QLineEdit>
#include <QTimer>

namespace {
	// Constants for buffer length configuration
	constexpr int MIN_BUFFER_LENGTH = 10;      // 10 seconds minimum
	constexpr int MAX_BUFFER_LENGTH = 21600;    // 6 hours maximum
	constexpr int DEFAULT_BUFFER_LENGTH = 300; // 5 minutes default
	
	// Configuration keys
	constexpr const char* REPLAY_BUFFER_LENGTH_KEY = "RecRBTime";
	constexpr const char* DOCK_AREA_KEY = "DockArea";
	constexpr const char* DOCK_GEOMETRY_KEY = "DockGeometry";
	constexpr const char* MODULE_NAME = "replay_buffer_pro";

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
	, secondsEdit(nullptr)
	, saveFullBufferBtn(nullptr)
	, sliderDebounceTimer(new QTimer(this)) {
	
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
 * Automatically adds the widget to the left dock area.
 */
ReplayBufferPro::ReplayBufferPro(QMainWindow *mainWindow)
	: QDockWidget(mainWindow)
	, slider(nullptr)
	, secondsEdit(nullptr)
	, saveFullBufferBtn(nullptr)
	, sliderDebounceTimer(new QTimer(this)) {
	
	// Set window properties
	setWindowTitle(obs_module_text("ReplayBufferPro"));
	setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

	// Initialize components and state
	initializeUI();
	loadCurrentBufferLength();
	connectSignalsAndSlots();
	
	// Register for OBS events
	obs_frontend_add_event_callback(OBSFrontendEvent, this);

	// Load saved dock state
	loadDockState(mainWindow);

	// Connect dock state changes to save function
	connect(this, &QDockWidget::dockLocationChanged, this, &ReplayBufferPro::saveDockState);
	connect(this, &QDockWidget::topLevelChanged, this, &ReplayBufferPro::saveDockState);
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
	sliderLayout->setAlignment(Qt::AlignTop); // Align contents to top
	
	// Initialize slider with range
	slider = new QSlider(Qt::Horizontal, container);
	slider->setRange(MIN_BUFFER_LENGTH, MAX_BUFFER_LENGTH);
	
	// Initialize seconds edit box
	secondsEdit = new QLineEdit(container);
	secondsEdit->setFixedWidth(60);  // Set a reasonable width
	secondsEdit->setAlignment(Qt::AlignRight);  // Right-align the text
	secondsEdit->setPlaceholderText("s");  // Show "s" as placeholder when empty
	
	// Add slider components to layout
	sliderLayout->addWidget(slider);
	sliderLayout->addWidget(secondsEdit);
	mainLayout->addLayout(sliderLayout);

	// Add spacing between slider and save buttons
	mainLayout->addSpacing(10);

	// Add "Save Clip" header
	mainLayout->addWidget(new QLabel(obs_module_text("SaveClip"), container));

	// Create and add save buttons section
	QHBoxLayout *buttonLayout = new QHBoxLayout();
	initializeSaveButtons(buttonLayout);
	mainLayout->addLayout(buttonLayout);

	// Add stretch to push everything to the top
	mainLayout->addStretch();

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

	// Create a grid layout for the buttons
	QGridLayout* gridLayout = new QGridLayout();
	gridLayout->setSpacing(5);  // Space between buttons
	
	// Calculate a reasonable number of columns (4 buttons per row)
	const int buttonsPerRow = 4;
	
	// Create buttons for each predefined duration
	for (size_t i = 0; i < sizeof(SAVE_BUTTONS)/sizeof(SAVE_BUTTONS[0]); i++) {
		const auto& btn = SAVE_BUTTONS[i];
		// Create button with translated text
		auto button = new QPushButton(obs_module_text(btn.text), this);
		button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		
		// Connect button click to save function with specific duration
		connect(button, &QPushButton::clicked, this, [this, duration = btn.duration]() {
			saveReplaySegment(duration);
		});
		
		// Add to grid layout
		int row = i / buttonsPerRow;
		int col = i % buttonsPerRow;
		gridLayout->addWidget(button, row, col);
		
		// Add to tracking vector
		saveButtons.push_back(button);
	}

	// Create and add the full buffer save button
	saveFullBufferBtn = new QPushButton(obs_module_text("SaveFull"), this);
	saveFullBufferBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	
	// Add the full buffer button on its own row
	int lastRow = (saveButtons.size() - 1) / buttonsPerRow + 1;
	gridLayout->addWidget(saveFullBufferBtn, lastRow, 0, 1, buttonsPerRow);
	
	connect(saveFullBufferBtn, &QPushButton::clicked, this, &ReplayBufferPro::saveFullBuffer);

	// Add the grid layout to the main layout
	layout->addLayout(gridLayout);
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
	
	// Get current replay buffer length (stored in seconds)
	uint64_t replayBufferLength = config_get_uint(config, section, REPLAY_BUFFER_LENGTH_KEY);
	
	// Set length, defaulting if current value is invalid
	setBufferLength(replayBufferLength > 0 ? static_cast<int>(replayBufferLength) : DEFAULT_BUFFER_LENGTH);
}

/**
 * @brief Updates the buffer length in the UI
 * @param seconds New buffer length in seconds
 * 
 * Updates:
 * - Slider position
 * - Seconds edit box text
 * - Save button states based on new length
 */
void ReplayBufferPro::setBufferLength(int seconds) {
	// Update UI components
	slider->setValue(seconds);
	secondsEdit->setText(QString::number(seconds));
	
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
 * - Text edit changes -> Buffer length updates
 */
void ReplayBufferPro::connectSignalsAndSlots() {
	// Configure debounce timer
	sliderDebounceTimer->setSingleShot(true);
	sliderDebounceTimer->setInterval(500); // 500ms debounce

	// Connect slider value changes to immediate UI updates
	connect(slider, &QSlider::valueChanged, this, [this](int value) {
		setBufferLength(value);
		sliderDebounceTimer->start(); // Restart debounce timer
	});

	// Connect debounce timer timeout to settings update
	connect(sliderDebounceTimer, &QTimer::timeout, 
			this, &ReplayBufferPro::handleDebouncedSliderChange);

	// Connect text edit changes
	connect(secondsEdit, &QLineEdit::editingFinished, this, &ReplayBufferPro::handleTextInput);
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

		// Skip update if value hasn't changed
		if (config_get_uint(config, section, REPLAY_BUFFER_LENGTH_KEY) == seconds) {
			return;
		}

		// Update OBS configuration (length is in seconds)
		config_set_uint(config, section, REPLAY_BUFFER_LENGTH_KEY, seconds);
		config_save(config);

		// Update the active replay buffer output (runtime setting)
		// This is needed because changing the config alone doesn't affect the running buffer
		if (obs_output_t* replay_output = obs_frontend_get_replay_buffer_output()) {
			OBSDataRAII settings(obs_output_get_settings(replay_output));
			if (settings.isValid()) {
				obs_data_set_int(settings.get(), "max_time_sec", seconds);
				obs_output_update(replay_output, settings.get());
			}
			obs_output_release(replay_output);
		}

		// Save all OBS settings
		obs_frontend_save();

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
	
	// Get current buffer length
	uint64_t currentBufferLength = config_get_uint(config, section, REPLAY_BUFFER_LENGTH_KEY);

	// Verify requested duration is valid
	if (duration > static_cast<int>(currentBufferLength)) {
		// Show warning if duration exceeds buffer length
		QMessageBox::warning(this, obs_module_text("Warning"),
			QString(obs_module_text("CannotSaveSegment"))
				.arg(duration)
				.arg(currentBufferLength));
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
 * - Disables slider and text edit when buffer is active
 * - Updates current buffer length when active
 */
void ReplayBufferPro::updateSliderState() {
	// Check current buffer state
	bool isActive = obs_frontend_replay_buffer_active();
	
	// Disable slider and text edit while buffer is active to prevent changes
	slider->setEnabled(!isActive);
	secondsEdit->setEnabled(!isActive);
	
	// Update current length if buffer is active
	if (isActive) {
		loadCurrentBufferLength();
	}
}

/**
 * @brief Handles text input from the seconds edit box
 * 
 * Validates the input and updates the buffer length accordingly.
 */
void ReplayBufferPro::handleTextInput() {
	bool ok;
	int value = secondsEdit->text().toInt(&ok);
	
	// Validate input
	if (!ok || value < MIN_BUFFER_LENGTH || value > MAX_BUFFER_LENGTH) {
		// Invalid input - reset to current slider value
		setBufferLength(slider->value());
		return;
	}

	// Valid input - update slider and settings
	slider->setValue(value);
	updateReplayBufferSettings(value);
}

/**
 * @brief Handles debounced slider value changes
 * 
 * Called after the slider stops moving for the debounce period.
 * Updates the OBS settings with the new buffer length.
 */
void ReplayBufferPro::handleDebouncedSliderChange() {
	// Update OBS settings with current slider value
	updateReplayBufferSettings(slider->value());
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

void ReplayBufferPro::loadDockState(QMainWindow *mainWindow) {
	// Get config file path
	char* config_path = obs_module_config_path("dock_state.json");
	if (!config_path) {
		// Default to left dock area if can't get config path
		mainWindow->addDockWidget(Qt::LeftDockWidgetArea, this);
		return;
	}

	// Create data from JSON file with backup extension
	OBSDataRAII data(obs_data_create_from_json_file(config_path));
	bfree(config_path); // Free the allocated path

	if (!data.isValid()) {
		// Default to left dock area if no saved state
		mainWindow->addDockWidget(Qt::LeftDockWidgetArea, this);
		return;
	}

	// Load dock area
	Qt::DockWidgetArea area = static_cast<Qt::DockWidgetArea>(
		obs_data_get_int(data.get(), DOCK_AREA_KEY));
	
	// Default to left if invalid area
	if (area != Qt::LeftDockWidgetArea && 
		area != Qt::RightDockWidgetArea && 
		area != Qt::TopDockWidgetArea && 
		area != Qt::BottomDockWidgetArea) {
		area = Qt::LeftDockWidgetArea;
	}

	// Add to saved area
	mainWindow->addDockWidget(area, this);

	// Restore geometry if floating
	QByteArray geometry = QByteArray::fromBase64(
		obs_data_get_string(data.get(), DOCK_GEOMETRY_KEY));
	if (!geometry.isEmpty()) {
		restoreGeometry(geometry);
	}
}

void ReplayBufferPro::saveDockState() {
	OBSDataRAII data(obs_data_create());
	if (!data.isValid()) return;

	// Save dock area
	Qt::DockWidgetArea area = Qt::NoDockWidgetArea;
	if (QMainWindow *mainWindow = qobject_cast<QMainWindow*>(parent())) {
		area = mainWindow->dockWidgetArea(this);
	}
	obs_data_set_int(data.get(), DOCK_AREA_KEY, static_cast<int>(area));

	// Save geometry if floating
	if (isFloating()) {
		QByteArray geometry = saveGeometry().toBase64();
		obs_data_set_string(data.get(), DOCK_GEOMETRY_KEY, geometry.constData());
	}

	// Get config directory path
	char* config_dir = obs_module_config_path("");
	if (!config_dir) {
		blog(LOG_WARNING, "Failed to get config directory path");
		return;
	}

	// Ensure config directory exists
	if (os_mkdirs(config_dir) < 0) {
		blog(LOG_WARNING, "Failed to create config directory: %s", config_dir);
		bfree(config_dir);
		return;
	}

	// Build full config file path
	std::string config_path = std::string(config_dir) + "/dock_state.json";
	bfree(config_dir);

	// Save to file with safe write
	if (!obs_data_save_json_safe(data.get(), config_path.c_str(), "tmp", "bak")) {
		blog(LOG_WARNING, "Failed to save dock state to: %s", config_path.c_str());
	} else {
		blog(LOG_INFO, "Saved dock state to: %s", config_path.c_str());
	}
}
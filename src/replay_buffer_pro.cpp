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

//=============================================================================
// CONSTRUCTORS & DESTRUCTOR
//=============================================================================
// Handles object lifecycle, initialization, and cleanup

/**
 * @brief Standalone widget constructor
 * @param parent Parent widget for memory management
 * 
 * Creates a floating/dockable widget that can be added to any Qt window.
 * Initializes all UI components and sets up event handling.
 */
ReplayBufferPro::ReplayBufferPro(QWidget *parent)
	: QDockWidget(parent)
	, slider(nullptr)
	, secondsEdit(nullptr)
	, saveFullBufferBtn(nullptr)
	, sliderDebounceTimer(new QTimer(this))
	, settingsMonitorTimer(new QTimer(this))
	, lastKnownBufferLength(0) {
	
	setWindowTitle(obs_module_text("ReplayBufferPro"));
	setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

	initUI();
	loadBufferLength();
	initSignals();
	
	obs_frontend_add_event_callback(handleOBSEvent, this);

	// Setup settings monitoring
	settingsMonitorTimer->setInterval(1000); // Check every second
	connect(settingsMonitorTimer, &QTimer::timeout, this, [this]() {
		config_t* config = obs_frontend_get_profile_config();
		if (!config) return;

		const char* mode = config_get_string(config, "Output", "Mode");
		const char* section = (mode && strcmp(mode, "Advanced") == 0) ? "AdvOut" : "SimpleOutput";
		
		uint64_t currentBufferLength = config_get_uint(config, section, REPLAY_BUFFER_LENGTH_KEY);
		
		if (currentBufferLength != lastKnownBufferLength && currentBufferLength > 0) {
			lastKnownBufferLength = currentBufferLength;
			updateBufferLengthUI(static_cast<int>(currentBufferLength));
			blog(LOG_INFO, "Detected buffer length change in OBS settings: %llu seconds", currentBufferLength);
		}
	});
	settingsMonitorTimer->start();
}

/**
 * @brief Main window constructor
 * @param mainWindow OBS main window to dock to
 * 
 * Creates a widget docked to the OBS main window.
 * Restores previous dock position and state if available.
 */
ReplayBufferPro::ReplayBufferPro(QMainWindow *mainWindow)
	: QDockWidget(mainWindow)
	, slider(nullptr)
	, secondsEdit(nullptr)
	, saveFullBufferBtn(nullptr)
	, sliderDebounceTimer(new QTimer(this))
	, settingsMonitorTimer(new QTimer(this))
	, lastKnownBufferLength(0) {
	
	setWindowTitle(obs_module_text("ReplayBufferPro"));
	setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

	initUI();
	loadBufferLength();
	initSignals();
	
	obs_frontend_add_event_callback(handleOBSEvent, this);
	loadDockState(mainWindow);

	connect(this, &QDockWidget::dockLocationChanged, this, &ReplayBufferPro::saveDockState);
	connect(this, &QDockWidget::topLevelChanged, this, &ReplayBufferPro::saveDockState);

	// Setup settings monitoring
	settingsMonitorTimer->setInterval(1000); // Check every second
	connect(settingsMonitorTimer, &QTimer::timeout, this, [this]() {
		config_t* config = obs_frontend_get_profile_config();
		if (!config) return;

		const char* mode = config_get_string(config, "Output", "Mode");
		const char* section = (mode && strcmp(mode, "Advanced") == 0) ? "AdvOut" : "SimpleOutput";
		
		uint64_t currentBufferLength = config_get_uint(config, section, REPLAY_BUFFER_LENGTH_KEY);
		
		if (currentBufferLength != lastKnownBufferLength && currentBufferLength > 0) {
			lastKnownBufferLength = currentBufferLength;
			updateBufferLengthUI(static_cast<int>(currentBufferLength));
			blog(LOG_INFO, "Detected buffer length change in OBS settings: %llu seconds", currentBufferLength);
		}
	});
	settingsMonitorTimer->start();
}

/**
 * @brief Destructor
 * 
 * Cleans up resources and removes OBS event callbacks.
 * Ensures no dangling callbacks remain after destruction.
 */
ReplayBufferPro::~ReplayBufferPro() {
	settingsMonitorTimer->stop();
	obs_frontend_remove_event_callback(handleOBSEvent, this);
}

/**
 * @brief OBS frontend event handler
 * @param event Type of OBS event that occurred
 * @param ptr Pointer to the ReplayBufferPro instance
 * 
 * Handles OBS events related to replay buffer state changes.
 * Uses Qt's event system to safely update UI from any thread.
 */
void ReplayBufferPro::handleOBSEvent(enum obs_frontend_event event, void *ptr) {
	auto window = static_cast<ReplayBufferPro*>(ptr);
	
	switch (event) {
		case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTING:
			window->settingsMonitorTimer->stop(); // Stop monitoring when buffer starts
			QMetaObject::invokeMethod(window, "updateUIState", Qt::QueuedConnection);
			break;
		case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPING:
			QMetaObject::invokeMethod(window, "updateUIState", Qt::QueuedConnection);
			break;
		case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED:
			QMetaObject::invokeMethod(window, "updateUIState", Qt::QueuedConnection);
			break;
		case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED:
			window->settingsMonitorTimer->start(); // Resume monitoring when buffer stops
			QMetaObject::invokeMethod(window, "updateUIState", Qt::QueuedConnection);
			// Reload buffer length in case it was changed while buffer was active
			QMetaObject::invokeMethod(window, "loadBufferLength", Qt::QueuedConnection);
			break;
		default:
			break;
	}
}

//=============================================================================
// EVENT HANDLERS
//=============================================================================
// Handles user interactions and UI events

/**
 * @brief Slider value change handler
 * @param value New slider position in seconds
 * 
 * Updates UI immediately and starts debounce timer for OBS settings update.
 * Prevents rapid OBS settings updates during slider movement.
 */
void ReplayBufferPro::handleSliderChanged(int value) {
	updateBufferLengthUI(value);
	sliderDebounceTimer->start();
}

/**
 * @brief Slider movement finished handler
 * 
 * Called after slider movement stops and debounce period expires.
 * Updates OBS settings with the final slider value.
 */
void ReplayBufferPro::handleSliderFinished() {
	updateOBSBufferLength(slider->value());
}

/**
 * @brief Text input handler for buffer length
 * 
 * Validates manual input for buffer length:
 * - Ensures value is a valid number
 * - Checks range (10s to 6h)
 * - Updates UI and settings if valid
 * - Reverts to previous value if invalid
 */
void ReplayBufferPro::handleBufferLengthInput() {
	bool ok;
	int value = secondsEdit->text().toInt(&ok);
	
	if (!ok || value < MIN_BUFFER_LENGTH || value > MAX_BUFFER_LENGTH) {
		updateBufferLengthUI(slider->value());
		return;
	}

	slider->setValue(value);
	updateOBSBufferLength(value);
}

/**
 * @brief Full buffer save handler
 * 
 * Triggers saving of entire replay buffer content.
 * Shows error if buffer is not active.
 */
void ReplayBufferPro::handleSaveFullBuffer() {
	if (obs_frontend_replay_buffer_active()) {
		obs_frontend_replay_buffer_save();
	} else {
		QMessageBox::warning(this, obs_module_text("Error"), 
			obs_module_text("ReplayBufferNotActive"));
	}
}

/**
 * @brief Segment save handler
 * @param duration Number of seconds to save
 * 
 * Saves a specific duration from the replay buffer:
 * - Verifies buffer is active
 * - Checks if duration exceeds buffer length
 * - Shows appropriate error messages
 * - Triggers save if all checks pass
 */
void ReplayBufferPro::handleSaveSegment(int duration) {
	if (!obs_frontend_replay_buffer_active()) {
		QMessageBox::warning(this, obs_module_text("Warning"), 
			obs_module_text("ReplayBufferNotActive"));
		return;
	}

	config_t* config = obs_frontend_get_profile_config();
	if (!config) return;

	const char* mode = config_get_string(config, "Output", "Mode");
	const char* section = (mode && strcmp(mode, "Advanced") == 0) ? "AdvOut" : "SimpleOutput";
	
	uint64_t currentBufferLength = config_get_uint(config, section, REPLAY_BUFFER_LENGTH_KEY);

	if (duration > static_cast<int>(currentBufferLength)) {
		QMessageBox::warning(this, obs_module_text("Warning"),
			QString(obs_module_text("CannotSaveSegment"))
				.arg(duration)
				.arg(currentBufferLength));
		return;
	}

	obs_frontend_replay_buffer_save();
	blog(LOG_INFO, "Saving replay segment of %d seconds", duration);
}

//=============================================================================
// INITIALIZATION
//=============================================================================
// Sets up UI components and establishes connections

/**
 * @brief Main UI initialization
 * 
 * Creates and arranges all UI components:
 * - Buffer length controls (slider + text input)
 * - Save duration buttons
 * - Full buffer save button
 * Uses vertical layout with appropriate spacing and alignment.
 */
void ReplayBufferPro::initUI() {
	QWidget *container = new QWidget(this);
	QVBoxLayout *mainLayout = new QVBoxLayout(container);

	mainLayout->addWidget(new QLabel(obs_module_text("BufferLength"), container));

	QHBoxLayout *sliderLayout = new QHBoxLayout();
	sliderLayout->setAlignment(Qt::AlignTop);
	
	slider = new QSlider(Qt::Horizontal, container);
	slider->setRange(MIN_BUFFER_LENGTH, MAX_BUFFER_LENGTH);
	
	secondsEdit = new QLineEdit(container);
	secondsEdit->setFixedWidth(60);
	secondsEdit->setAlignment(Qt::AlignRight);
	secondsEdit->setPlaceholderText("s");
	
	sliderLayout->addWidget(slider);
	sliderLayout->addWidget(secondsEdit);
	mainLayout->addLayout(sliderLayout);

	mainLayout->addSpacing(10);
	mainLayout->addWidget(new QLabel(obs_module_text("SaveClip"), container));

	QHBoxLayout *buttonLayout = new QHBoxLayout();
	initSaveButtons(buttonLayout);
	mainLayout->addLayout(buttonLayout);

	mainLayout->addStretch();
	setWidget(container);
}

/**
 * @brief Save button initialization
 * @param layout Parent layout to add buttons to
 * 
 * Creates grid of save duration buttons:
 * - Quick-save buttons for predefined durations
 * - Full buffer save button
 * - Configures button appearance and behavior
 * - Arranges buttons in grid layout (4 per row)
 */
void ReplayBufferPro::initSaveButtons(QHBoxLayout *layout) {
	saveButtons.clear();

	QGridLayout* gridLayout = new QGridLayout();
	gridLayout->setSpacing(5);
	
	const int buttonsPerRow = 4;
	
	for (size_t i = 0; i < sizeof(SAVE_BUTTONS)/sizeof(SAVE_BUTTONS[0]); i++) {
		const auto& btn = SAVE_BUTTONS[i];
		auto button = new QPushButton(obs_module_text(btn.text), this);
		button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		
		connect(button, &QPushButton::clicked, this, [this, duration = btn.duration]() {
			handleSaveSegment(duration);
		});
		
		int row = i / buttonsPerRow;
		int col = i % buttonsPerRow;
		gridLayout->addWidget(button, row, col);
		
		saveButtons.push_back(button);
	}

	saveFullBufferBtn = new QPushButton(obs_module_text("SaveFull"), this);
	saveFullBufferBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	
	int lastRow = (saveButtons.size() - 1) / buttonsPerRow + 1;
	gridLayout->addWidget(saveFullBufferBtn, lastRow, 0, 1, buttonsPerRow);
	
	connect(saveFullBufferBtn, &QPushButton::clicked, this, &ReplayBufferPro::handleSaveFullBuffer);

	layout->addLayout(gridLayout);
}

/**
 * @brief Signal/slot connection initialization
 * 
 * Sets up all event handling connections:
 * - Slider value changes (with debouncing)
 * - Text input validation
 * - Save button clicks
 * Configures debounce timer for slider updates.
 */
void ReplayBufferPro::initSignals() {
	sliderDebounceTimer->setSingleShot(true);
	sliderDebounceTimer->setInterval(500);

	connect(slider, &QSlider::valueChanged, this, &ReplayBufferPro::handleSliderChanged);
	connect(sliderDebounceTimer, &QTimer::timeout, this, &ReplayBufferPro::handleSliderFinished);
	connect(secondsEdit, &QLineEdit::editingFinished, this, &ReplayBufferPro::handleBufferLengthInput);
}

//=============================================================================
// STATE MANAGEMENT
//=============================================================================
// Handles UI state updates and synchronization

/**
 * @brief Updates UI with new buffer length
 * @param seconds New buffer length in seconds
 * 
 * Synchronizes UI components with new buffer length:
 * - Updates slider position
 * - Updates text input value
 * - Updates save button states
 */
void ReplayBufferPro::updateBufferLengthUI(int seconds) {
	slider->setValue(seconds);
	secondsEdit->setText(QString::number(seconds));
	
	toggleSaveButtons(seconds);
}

/**
 * @brief Updates OBS buffer length setting
 * @param seconds New buffer length in seconds
 * 
 * Updates buffer length in OBS:
 * - Updates configuration file
 * - Updates active output settings
 * - Handles both Simple and Advanced output modes
 * - Shows error message if update fails
 */
void ReplayBufferPro::updateOBSBufferLength(int seconds) {
	try {
		config_t* config = obs_frontend_get_profile_config();
		if (!config) {
			throw std::runtime_error("Failed to get OBS profile config");
		}

		const char* mode = config_get_string(config, "Output", "Mode");
		const char* section = (mode && strcmp(mode, "Advanced") == 0) ? "AdvOut" : "SimpleOutput";

		if (config_get_uint(config, section, REPLAY_BUFFER_LENGTH_KEY) == seconds) {
			return;
		}

		config_set_uint(config, section, REPLAY_BUFFER_LENGTH_KEY, seconds);
		config_save(config);

		if (obs_output_t* replay_output = obs_frontend_get_replay_buffer_output()) {
			OBSDataRAII settings(obs_output_get_settings(replay_output));
			if (settings.isValid()) {
				obs_data_set_int(settings.get(), "max_time_sec", seconds);
				obs_output_update(replay_output, settings.get());
			}
			obs_output_release(replay_output);
		}

		obs_frontend_save();

	} catch (const std::exception& e) {
		blog(LOG_ERROR, "Failed to update buffer length: %s", e.what());
		QMessageBox::warning(this, obs_module_text("Error"), 
			QString(obs_module_text("FailedToUpdateLength")).arg(e.what()));
	}
}

/**
 * @brief Updates UI based on buffer state
 * 
 * Synchronizes UI with replay buffer state:
 * - Enables/disables controls based on buffer activity
 * - Updates buffer length display when active
 */
void ReplayBufferPro::updateUIState() {
	bool isActive = obs_frontend_replay_buffer_active();
	
	slider->setEnabled(!isActive);
	secondsEdit->setEnabled(!isActive);
}

/**
 * @brief Updates save button enabled states
 * @param bufferLength Current buffer length in seconds
 * 
 * Enables/disables save buttons based on buffer length:
 * - Enables buttons for durations <= buffer length
 * - Disables buttons for durations > buffer length
 */
void ReplayBufferPro::toggleSaveButtons(int bufferLength) {
	for (size_t i = 0; i < saveButtons.size(); i++) {
		saveButtons[i]->setEnabled(bufferLength >= SAVE_BUTTONS[i].duration);
	}
}

//=============================================================================
// PERSISTENCE
//=============================================================================
// Handles loading and saving of settings

/**
 * @brief Loads buffer length from OBS settings
 * 
 * Retrieves and applies saved buffer length:
 * - Handles both Simple and Advanced output modes
 * - Falls back to default length (5m) if not set
 * - Updates UI with loaded value
 */
void ReplayBufferPro::loadBufferLength() {
	config_t* config = obs_frontend_get_profile_config();
	if (!config) {
		blog(LOG_ERROR, "Failed to get OBS profile config");
		updateBufferLengthUI(DEFAULT_BUFFER_LENGTH);
		return;
	}

	const char* mode = config_get_string(config, "Output", "Mode");
	const char* section = (mode && strcmp(mode, "Advanced") == 0) ? "AdvOut" : "SimpleOutput";
	
	uint64_t replayBufferLength = config_get_uint(config, section, REPLAY_BUFFER_LENGTH_KEY);
	lastKnownBufferLength = replayBufferLength; // Update the last known length
	
	updateBufferLengthUI(replayBufferLength > 0 ? static_cast<int>(replayBufferLength) : DEFAULT_BUFFER_LENGTH);
}

/**
 * @brief Loads dock widget state
 * @param mainWindow Main window to dock to
 * 
 * Restores saved dock position and state:
 * - Loads dock area (left, right, top, bottom)
 * - Restores floating state and geometry
 * - Falls back to left dock area if no saved state
 */
void ReplayBufferPro::loadDockState(QMainWindow *mainWindow) {
	char* config_path = obs_module_config_path("dock_state.json");
	if (!config_path) {
		mainWindow->addDockWidget(Qt::LeftDockWidgetArea, this);
		return;
	}

	OBSDataRAII data(obs_data_create_from_json_file(config_path));
	bfree(config_path);

	if (!data.isValid()) {
		mainWindow->addDockWidget(Qt::LeftDockWidgetArea, this);
		return;
	}

	Qt::DockWidgetArea area = static_cast<Qt::DockWidgetArea>(
		obs_data_get_int(data.get(), DOCK_AREA_KEY));
	
	if (area != Qt::LeftDockWidgetArea && 
		area != Qt::RightDockWidgetArea && 
		area != Qt::TopDockWidgetArea && 
		area != Qt::BottomDockWidgetArea) {
		area = Qt::LeftDockWidgetArea;
	}

	mainWindow->addDockWidget(area, this);

	QByteArray geometry = QByteArray::fromBase64(
		obs_data_get_string(data.get(), DOCK_GEOMETRY_KEY));
	if (!geometry.isEmpty()) {
		restoreGeometry(geometry);
	}
}

/**
 * @brief Saves dock widget state
 * 
 * Persists current dock position and state:
 * - Saves dock area
 * - Saves floating state and geometry
 * - Uses safe file writing with backup
 */
void ReplayBufferPro::saveDockState() {
	OBSDataRAII data(obs_data_create());
	if (!data.isValid()) return;

	Qt::DockWidgetArea area = Qt::NoDockWidgetArea;
	if (QMainWindow *mainWindow = qobject_cast<QMainWindow*>(parent())) {
		area = mainWindow->dockWidgetArea(this);
	}
	obs_data_set_int(data.get(), DOCK_AREA_KEY, static_cast<int>(area));

	if (isFloating()) {
		QByteArray geometry = saveGeometry().toBase64();
		obs_data_set_string(data.get(), DOCK_GEOMETRY_KEY, geometry.constData());
	}

	char* config_dir = obs_module_config_path("");
	if (!config_dir) {
		blog(LOG_WARNING, "Failed to get config directory path");
		return;
	}

	if (os_mkdirs(config_dir) < 0) {
		blog(LOG_WARNING, "Failed to create config directory: %s", config_dir);
		bfree(config_dir);
		return;
	}

	std::string config_path = std::string(config_dir) + "/dock_state.json";
	bfree(config_dir);

	if (!obs_data_save_json_safe(data.get(), config_path.c_str(), "tmp", "bak")) {
		blog(LOG_WARNING, "Failed to save dock state to: %s", config_path.c_str());
	} else {
		blog(LOG_INFO, "Saved dock state to: %s", config_path.c_str());
	}
}
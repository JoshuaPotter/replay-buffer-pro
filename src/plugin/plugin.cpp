/**
 * @file plugin.cpp
 * @brief Implementation of the Replay Buffer Pro plugin
 *
 * Provides enhanced replay buffer controls for OBS Studio including:
 * - Configurable buffer length adjustment
 * - Segment-based replay saving
 * - Automatic replay trimming
 */

// OBS includes
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>
#include <util/platform.h>

// Qt includes
#include <QMessageBox>
#include <QTimer>

// Local includes
#include "utils/obs-utils.hpp"
#include "plugin/plugin.hpp"
#include "utils/logger.hpp"
#include "config/config.hpp"	

namespace ReplayBufferPro
{

	//=============================================================================
	// CONSTRUCTORS & DESTRUCTOR
	//=============================================================================

	/**
	 * @brief Creates a standalone widget instance
	 * @param parent Parent widget
	 *
	 * Creates a floating/dockable widget that can be added to any Qt window.
	 * Initializes all UI components and sets up event handling.
	 */
	Plugin::Plugin(QWidget *parent)
			: QDockWidget(parent), 
			  lastKnownBufferLength(0)
	{
		setWindowTitle(obs_module_text("ReplayBufferPro"));
		setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

		// Create component instances
		replayManager = new ReplayBufferManager(this);
		settingsManager = new SettingsManager();
		
		// Create UI components with callbacks
		ui = new UIComponents(this, 
			[this](int duration) { handleSaveSegment(duration); },
			[this]() { handleSaveFullBuffer(); }
		);
		
		// Set the widget
		setWidget(ui->createUI());
		
		// Initialize signals and load settings
		initSignals();
		loadBufferLength();

		// Register OBS event callback
		obs_frontend_add_event_callback(handleOBSEvent, this);

		// Setup settings monitoring
		settingsMonitorTimer = new QTimer(this);
		settingsMonitorTimer->setInterval(Config::SETTINGS_MONITOR_INTERVAL);
		connect(settingsMonitorTimer, &QTimer::timeout, this, [this]() {
			int currentBufferLength = settingsManager->getCurrentBufferLength();
			
			if (currentBufferLength != lastKnownBufferLength && currentBufferLength > 0) {
				lastKnownBufferLength = currentBufferLength;
				ui->updateBufferLengthValue(currentBufferLength);
				Logger::info("Buffer length changed in OBS settings: %d seconds", currentBufferLength);
			}
		});
		settingsMonitorTimer->start();
	}

	/**
	 * @brief Creates a docked widget instance
	 * @param mainWindow OBS main window
	 *
	 * Creates a widget docked to the OBS main window.
	 * Restores previous dock position and state if available.
	 */
	Plugin::Plugin(QMainWindow *mainWindow)
			: QDockWidget(mainWindow), 
			  lastKnownBufferLength(0)
	{
		setWindowTitle(obs_module_text("ReplayBufferPro"));
		setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

		// Create component instances
		replayManager = new ReplayBufferManager(this);
		settingsManager = new SettingsManager();
		dockStateManager = new DockStateManager(this);
		
		// Create UI components with callbacks
		ui = new UIComponents(this, 
			[this](int duration) { handleSaveSegment(duration); },
			[this]() { handleSaveFullBuffer(); }
		);
		
		// Set the widget
		setWidget(ui->createUI());
		
		// Initialize signals and load settings
		initSignals();
		loadBufferLength();

		// Register OBS event callback
		obs_frontend_add_event_callback(handleOBSEvent, this);
		
		// Load dock state
		dockStateManager->loadDockState(mainWindow);

		// Connect dock state signals
		connect(this, &QDockWidget::dockLocationChanged, this, [this]() {
			dockStateManager->saveDockState();
		});
		connect(this, &QDockWidget::topLevelChanged, this, [this]() {
			dockStateManager->saveDockState();
		});

		// Setup settings monitoring
		settingsMonitorTimer = new QTimer(this);
		settingsMonitorTimer->setInterval(Config::SETTINGS_MONITOR_INTERVAL);
		connect(settingsMonitorTimer, &QTimer::timeout, this, [this]() {
			int currentBufferLength = settingsManager->getCurrentBufferLength();
			
			if (currentBufferLength != lastKnownBufferLength && currentBufferLength > 0) {
				lastKnownBufferLength = currentBufferLength;
				ui->updateBufferLengthValue(currentBufferLength);
				Logger::info("Buffer length changed in OBS settings: %d seconds", currentBufferLength);
			}
		});
		settingsMonitorTimer->start();
	}

	/**
	 * @brief Cleans up resources and removes callbacks
	 *
	 * Cleans up resources and removes OBS event callbacks.
	 * Ensures no dangling callbacks remain after destruction.
	 */
	Plugin::~Plugin()
	{
		settingsMonitorTimer->stop();
		obs_frontend_remove_event_callback(handleOBSEvent, this);
		
		// Component cleanup happens automatically through Qt parent-child relationship
	}

	//=============================================================================
	// INITIALIZATION
	//=============================================================================

	/**
	 * @brief Initializes signal/slot connections
	 *
	 * Sets up all event handling connections:
	 * - Slider value changes (with debouncing)
	 * - Text input validation
	 * - Save button clicks
	 * Configures debounce timer for slider updates.
	 */
	void Plugin::initSignals()
	{
		connect(ui->getSlider(), &QSlider::valueChanged, this, &Plugin::handleSliderChanged);
		connect(ui->getSliderDebounceTimer(), &QTimer::timeout, this, &Plugin::handleSliderFinished);
		connect(ui->getSecondsEdit(), &QLineEdit::editingFinished, this, &Plugin::handleBufferLengthInput);
	}

	//=============================================================================
	// EVENT HANDLERS
	//=============================================================================

	/**
	 * @brief Handles OBS frontend events
	 * @param event The OBS event type
	 * @param ptr Pointer to the plugin instance
	 *
	 * Handles OBS events related to replay buffer state changes.
	 * Uses Qt's event system to safely update UI from any thread.
	 */
	void Plugin::handleOBSEvent(enum obs_frontend_event event, void *ptr)
	{
		auto window = static_cast<Plugin *>(ptr);

		switch (event)
		{
		case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTING:
			window->settingsMonitorTimer->stop(); // Stop monitoring when buffer starts
			QMetaObject::invokeMethod(window, "updateBufferLengthUIState", Qt::QueuedConnection);
			break;
		case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPING:
			QMetaObject::invokeMethod(window, "updateBufferLengthUIState", Qt::QueuedConnection);
			break;
		case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED:
			QMetaObject::invokeMethod(window, "updateBufferLengthUIState", Qt::QueuedConnection);
			break;
		case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED:
			window->settingsMonitorTimer->start(); // Resume monitoring when buffer stops
			QMetaObject::invokeMethod(window, "updateBufferLengthUIState", Qt::QueuedConnection);
			// Reload buffer length in case it was changed while buffer was active
			QMetaObject::invokeMethod(window, "loadBufferLength", Qt::QueuedConnection);
			break;
		case OBS_FRONTEND_EVENT_REPLAY_BUFFER_SAVED:
			if (window->replayManager->getPendingSaveDuration() > 0)
			{
				const char *savedPath = obs_frontend_get_last_replay();
				if (savedPath)
				{
					int duration = window->replayManager->getPendingSaveDuration();
					Logger::info("Trimming replay buffer save to %d seconds", duration);
					window->replayManager->trimReplayBuffer(savedPath, duration);
					bfree((void *)savedPath);
				}
				window->replayManager->clearPendingSaveDuration();
			}
			break;
		default:
			break;
		}
	}

	/**
	 * @brief Updates UI and starts debounce timer when slider value changes
	 * @param value New buffer length in seconds
	 *
	 * Updates UI immediately and starts debounce timer for OBS settings update.
	 * Prevents rapid OBS settings updates during slider movement.
	 */
	void Plugin::handleSliderChanged(int value)
	{
		ui->updateBufferLengthValue(value);
		ui->getSliderDebounceTimer()->start();
	}

	/**
	 * @brief Updates OBS settings after slider movement ends
	 *
	 * Called after slider movement stops and debounce period expires.
	 * Updates OBS settings with the final slider value.
	 */
	void Plugin::handleSliderFinished()
	{
		try {
			settingsManager->updateBufferLengthSettings(ui->getSlider()->value());
		} catch (const std::exception &e) {
			QMessageBox::warning(this, obs_module_text("Error"),
								QString(obs_module_text("FailedToUpdateLength")).arg(e.what()));
		}
	}

	/**
	 * @brief Validates and applies manual buffer length input
	 *
	 * Ensures input is within valid range (10s to 6h) and updates settings
	 */
	void Plugin::handleBufferLengthInput()
	{
		bool ok;
		int value = ui->getSecondsEdit()->text().toInt(&ok);

		if (!ok || value < Config::MIN_BUFFER_LENGTH || value > Config::MAX_BUFFER_LENGTH)
		{
			ui->updateBufferLengthValue(ui->getSlider()->value());
			return;
		}

		ui->getSlider()->setValue(value);
		try {
			settingsManager->updateBufferLengthSettings(value);
		} catch (const std::exception &e) {
			QMessageBox::warning(this, obs_module_text("Error"),
								QString(obs_module_text("FailedToUpdateLength")).arg(e.what()));
		}
	}

	/**
	 * @brief Triggers full buffer save if replay buffer is active
	 *
	 * Triggers saving of entire replay buffer content.
	 * Shows error if buffer is not active.
	 */
	void Plugin::handleSaveFullBuffer()
	{
		replayManager->saveFullBuffer(this);
	}

	/**
	 * @brief Saves a specific duration from the replay buffer
	 * @param duration Number of seconds to save
	 *
	 * Saves a specific duration from the replay buffer:
	 * - Verifies buffer is active
	 * - Checks if duration exceeds buffer length
	 * - Shows appropriate error messages
	 * - Triggers save if all checks pass
	 */
	void Plugin::handleSaveSegment(int duration)
	{
		replayManager->saveSegment(duration, this);
	}

	//=============================================================================
	// UI STATE MANAGEMENT
	//=============================================================================

	/**
	 * @brief Updates UI state based on replay buffer activity
	 *
	 * Synchronizes UI with replay buffer state:
	 * - Enables/disables controls based on buffer activity
	 * - Updates buffer length display when active
	 */
	void Plugin::updateBufferLengthUIState()
	{
		bool isActive = obs_frontend_replay_buffer_active();
		ui->updateBufferLengthState(isActive);
	}

	//=============================================================================
	// SETTINGS MANAGEMENT
	//=============================================================================

	/**
	 * @brief Loads and applies saved buffer length from OBS settings
	 *
	 * Retrieves and applies saved buffer length:
	 * - Handles both Simple and Advanced output modes
	 * - Falls back to default length (5m) if not set
	 * - Updates UI with loaded value
	 */
	void Plugin::loadBufferLength()
	{
		int bufferLength = settingsManager->loadBufferLength();
		lastKnownBufferLength = bufferLength;
		ui->updateBufferLengthValue(bufferLength);
	}

} // namespace ReplayBufferPro
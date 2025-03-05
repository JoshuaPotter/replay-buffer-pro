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
#include "config/config.hpp"	

namespace ReplayBufferPro
{
	//=============================================================================
	// CONSTRUCTORS & DESTRUCTOR
	//=============================================================================

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

		// Create and register hotkeys
		hotkeyManager = new HotkeyManager(
			[this](int duration) { handleSaveSegment(duration); }
		);
		hotkeyManager->registerHotkeys();

		// Setup settings monitoring
		settingsMonitorTimer = new QTimer(this);
		settingsMonitorTimer->setInterval(Config::SETTINGS_MONITOR_INTERVAL);
		connect(settingsMonitorTimer, &QTimer::timeout, this, &Plugin::loadBufferLength);
		settingsMonitorTimer->start();
	}

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
		
		// Create and register hotkeys
		hotkeyManager = new HotkeyManager(
			[this](int duration) { handleSaveSegment(duration); }
		);
		hotkeyManager->registerHotkeys();
		
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
		connect(settingsMonitorTimer, &QTimer::timeout, this, &Plugin::loadBufferLength);
		settingsMonitorTimer->start();
	}

	Plugin::~Plugin()
	{
		// Stop the settings monitor timer first
		if (settingsMonitorTimer) {
			settingsMonitorTimer->stop();
		}
		
		// Remove OBS callbacks before destroying components
		obs_frontend_remove_event_callback(handleOBSEvent, this);
		
		// No need for explicit manager deletion since destructors are defaulted
		
		// Qt parent-child relationship will handle cleanup
	}

	//=============================================================================
	// INITIALIZATION
	//=============================================================================

	void Plugin::initSignals()
	{
		connect(ui->getSlider(), &QSlider::valueChanged, this, &Plugin::handleSliderChanged);
		connect(ui->getSliderDebounceTimer(), &QTimer::timeout, this, &Plugin::handleSliderFinished);
		connect(ui->getSecondsEdit(), &QLineEdit::editingFinished, this, &Plugin::handleBufferLengthInput);
	}

	//=============================================================================
	// EVENT HANDLERS
	//=============================================================================

	void Plugin::handleOBSEvent(enum obs_frontend_event event, void *ptr)
	{
		auto plugin = static_cast<Plugin *>(ptr);

		switch (event)
		{
		case OBS_FRONTEND_EVENT_EXIT:
			plugin->settingsMonitorTimer->stop();
			break;
		case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTING:
			plugin->settingsMonitorTimer->stop();
			QMetaObject::invokeMethod(plugin, "updateBufferLengthUIState", Qt::QueuedConnection);
			break;
		case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED:
			plugin->settingsMonitorTimer->start();
			QMetaObject::invokeMethod(plugin, "updateBufferLengthUIState", Qt::QueuedConnection);
			QMetaObject::invokeMethod(plugin, "loadBufferLength", Qt::QueuedConnection);
			break;
		case OBS_FRONTEND_EVENT_REPLAY_BUFFER_SAVED:
			plugin->handleReplayBufferSaved();
			break;
		default:
			break;
		}
	}

	void Plugin::handleSliderChanged(int value)
	{
		// Only update the UI components, don't trigger settings update yet
		ui->updateBufferLengthValue(value);
		
		// Restart the debounce timer
		ui->getSliderDebounceTimer()->start();
	}

	void Plugin::handleSliderFinished()
	{
		// Get the final slider value
		int value = ui->getSlider()->value();
		
		// Only update settings if the value has actually changed
		if (value != lastKnownBufferLength)
		{
			try {
				settingsManager->updateBufferLengthSettings(value);
				lastKnownBufferLength = value;
			} catch (const std::exception &e) {
				QMessageBox::warning(this, obs_module_text("Error"),
									QString(obs_module_text("FailedToUpdateLength")).arg(e.what()));
			}
		}
	}

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

	void Plugin::handleSaveFullBuffer()
	{
		replayManager->saveFullBuffer(this);
	}

	void Plugin::handleSaveSegment(int duration)
	{
		replayManager->saveSegment(duration, this);
	}

	void Plugin::handleReplayBufferSaved() 
	{
		int duration = replayManager->getPendingSaveDuration();
		if (duration > 0) {
			const char* savedPath = obs_frontend_get_last_replay();
			if (savedPath) {
				replayManager->trimReplayBuffer(savedPath, duration);
				bfree((void*)savedPath);
			}
			replayManager->clearPendingSaveDuration();
		}
	}

	//=============================================================================
	// UI STATE MANAGEMENT
	//=============================================================================

	void Plugin::updateBufferLengthUIState()
	{
		bool isActive = obs_frontend_replay_buffer_active();
		ui->updateBufferLengthState(isActive);
	}

	//=============================================================================
	// SETTINGS MANAGEMENT
	//=============================================================================

	void Plugin::loadBufferLength()
	{
		int bufferLength = settingsManager->getCurrentBufferLength();
		if (bufferLength > 0 && bufferLength != lastKnownBufferLength)
		{
			lastKnownBufferLength = bufferLength;
			ui->updateBufferLengthValue(bufferLength);
		}
	}

} // namespace ReplayBufferPro
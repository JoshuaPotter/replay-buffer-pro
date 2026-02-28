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
#include <QVBoxLayout>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QPushButton>

// STL includes
#include <thread>
#include <string>
#include <vector>

// Local includes
#include "utils/obs-utils.hpp"
#include "plugin/plugin.hpp"
#include "config/config.hpp"
#include "utils/logger.hpp"

namespace ReplayBufferPro
{
	//=============================================================================
	// CONSTRUCTORS & DESTRUCTOR
	//=============================================================================

	Plugin::Plugin(QWidget *parent)
			: QWidget(parent), 
			  lastKnownBufferLength(0)
	{
		// Create component instances
		replayManager = new ReplayBufferManager(this);
		settingsManager = new SettingsManager();
		saveButtonSettings = new SaveButtonSettings();
		saveButtonSettings->load();
		
		// Create UI components with callbacks
		ui = new UIComponents(this, 
			[this](int duration) { handleSaveSegment(duration); },
			[this]() { handleSaveFullBuffer(); },
			[this]() { handleCustomizeSaveButtons(); }
		);
		ui->setSaveButtonDurations(saveButtonSettings->getDurations());
		
		// Mount the UI into this widget
		{
			QWidget *root = ui->createUI();
			auto *layout = new QVBoxLayout(this);
			layout->setContentsMargins(0, 0, 0, 0);
			layout->setSpacing(0);
			layout->addWidget(root);
			setLayout(layout);
		}
		
		// Initialize signals and load settings
		initSignals();
		loadBufferLength();

		// Register OBS event callback
		obs_frontend_add_event_callback(handleOBSEvent, this);

		// Create and register hotkeys
		hotkeyManager = new HotkeyManager(
			[this](int duration) { handleSaveSegment(duration); },
			saveButtonSettings->getDurations()
		);
		hotkeyManager->registerHotkeys();

		// Setup settings monitoring
		settingsMonitorTimer = new QTimer(this);
		settingsMonitorTimer->setInterval(Config::SETTINGS_MONITOR_INTERVAL);
		connect(settingsMonitorTimer, &QTimer::timeout, this, &Plugin::loadBufferLength);
		settingsMonitorTimer->start();
	}

	// Removed QMainWindow-based constructor; OBS wraps QWidget into a dock

	Plugin::~Plugin()
	{
		// Stop the settings monitor timer first
		if (settingsMonitorTimer) {
			settingsMonitorTimer->stop();
		}
		
		// Remove OBS callbacks before destroying components
		obs_frontend_remove_event_callback(handleOBSEvent, this);
		
		// Clean up managers that were allocated with new
		delete hotkeyManager;
		delete saveButtonSettings;
		delete settingsManager;
		
		// Qt parent-child relationship will handle cleanup for other components
	}

	//=============================================================================
	// INITIALIZATION
	//=============================================================================

	void Plugin::initSignals()
	{
		// Both slider and spinbox changes will trigger handleSliderChanged
		connect(ui->getSlider(), &QSlider::valueChanged, this, &Plugin::handleSliderChanged);
		connect(ui->getSecondsEdit(), QOverload<int>::of(&QSpinBox::valueChanged), 
				this, &Plugin::handleSliderChanged);  // Use same handler
		
		// Single debounce timer for both controls
		connect(ui->getSliderDebounceTimer(), &QTimer::timeout, this, &Plugin::handleSliderFinished);
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
			if (plugin->settingsMonitorTimer) {
				plugin->settingsMonitorTimer->stop();
			}
			if (plugin->hotkeyManager) {
				plugin->hotkeyManager->saveHotkeySettings();
			}
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

	void Plugin::handleBufferLengthInput(int value)
	{
		if (value < Config::MIN_BUFFER_LENGTH || value > Config::MAX_BUFFER_LENGTH)
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
		// Consume the pending duration immediately (before spawning the thread) so that a
		// rapid second save event sees 0 and does not attempt to double-trim.
		int duration = replayManager->getPendingSaveDuration();
		if (duration > 0) {
			replayManager->clearPendingSaveDuration();

			const char* savedPath = obs_frontend_get_last_replay();
			if (savedPath) {
				std::string pathCopy(savedPath);
				bfree((void*)savedPath);

				// Offload trimming to background thread to avoid blocking OBS event thread.
				// duration is captured by value; clearPendingSaveDuration has already been
				// called above so the next save event can proceed independently.
				auto *manager = replayManager;
				std::thread([manager, path = std::move(pathCopy), duration]() {
					manager->trimReplayBuffer(path.c_str(), duration);
				}).detach();
			}
		}
	}

	void Plugin::handleCustomizeSaveButtons()
	{
		QDialog dialog(this);
		dialog.setWindowTitle(obs_module_text("CustomizeButtonsTitle"));
		QVBoxLayout *layout = new QVBoxLayout(&dialog);

		QFormLayout *formLayout = new QFormLayout();
		std::vector<QSpinBox *> inputs;
		inputs.reserve(Config::SAVE_BUTTON_COUNT);

		const auto &durations = saveButtonSettings->getDurations();
		for (size_t i = 0; i < Config::SAVE_BUTTON_COUNT; i++)
		{
			QSpinBox *spinBox = new QSpinBox(&dialog);
			spinBox->setRange(1, Config::MAX_BUFFER_LENGTH);
			spinBox->setSuffix(" sec");
			if (i < durations.size())
			{
				spinBox->setValue(durations[i]);
			}

			QString labelText = QString::fromUtf8(obs_module_text("SaveClipButtonLabel")).arg(i + 1);
			formLayout->addRow(labelText, spinBox);
			inputs.push_back(spinBox);
		}

		layout->addLayout(formLayout);

		QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
		buttonBox->button(QDialogButtonBox::Ok)->setText(obs_module_text("CustomizeButtonsSave"));
		buttonBox->button(QDialogButtonBox::Cancel)->setText(obs_module_text("CustomizeButtonsCancel"));
		connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
		connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
		layout->addWidget(buttonBox);

		if (dialog.exec() != QDialog::Accepted)
		{
			return;
		}

		std::vector<int> updatedDurations;
		updatedDurations.reserve(inputs.size());
		for (auto *input : inputs)
		{
			updatedDurations.push_back(input->value());
		}

		saveButtonSettings->setDurations(updatedDurations);
		if (!saveButtonSettings->save())
		{
			Logger::warning("Failed to save custom save button durations");
		}

		ui->setSaveButtonDurations(saveButtonSettings->getDurations());
		if (hotkeyManager)
		{
			hotkeyManager->setSaveButtonDurations(saveButtonSettings->getDurations());
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

/**
 * @file ui-components.cpp
 * @brief Implementation of UI components for the Replay Buffer Pro plugin
 */

#include "ui/ui-components.hpp"
#include "config/config.hpp"
#include "utils/duration-format.hpp"

// OBS includes
#include <obs-module.h>

// Qt includes
#include <QObject>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QPushButton>
#include <QGridLayout>
#include <QSizePolicy>
#include <QTimer>
#include <QFrame>
#include <QMessageBox>

// STL includes
#include <algorithm>

namespace ReplayBufferPro
{
  class BufferLengthEventFilter : public QObject {
  protected:
      bool eventFilter(QObject* obj, QEvent* event) override {
          if ((event->type() == QEvent::MouseButtonPress || 
               event->type() == QEvent::KeyPress) && 
              !static_cast<QWidget*>(obj)->isEnabled()) {
              QMessageBox::warning(
                  static_cast<QWidget*>(obj),
                  obs_module_text("Warning"),
                  obs_module_text("ReplayBufferActive"),
                  QMessageBox::Ok
              );
              return true;
          }
          return QObject::eventFilter(obj, event);
      }
  };

  //=============================================================================
  // CONSTRUCTORS & DESTRUCTOR
  //=============================================================================

  UIComponents::UIComponents(QWidget *parent,
                             std::function<void(int)> saveSegmentCallback,
                             std::function<void()> saveFullBufferCallback,
                             std::function<void()> customizeSaveButtonsCallback)
      : secondsEdit(nullptr),
        saveFullBufferBtn(nullptr),
        customizeSaveButtonsBtn(nullptr),
        bufferLengthDebounceTimer(new QTimer(parent)),
        onSaveSegment(saveSegmentCallback),
        onSaveFullBuffer(saveFullBufferCallback),
        onCustomizeSaveButtons(customizeSaveButtonsCallback)
  {
    if (!parent) {
        qWarning("UIComponents: parent widget cannot be null");
        return;
    }

    bufferLengthDebounceTimer->setSingleShot(true);
    bufferLengthDebounceTimer->setInterval(Config::BUFFER_LENGTH_DEBOUNCE_INTERVAL);
    saveButtonDurations = getDefaultSaveButtonDurations();
  }

  //=============================================================================
  // UI CREATION
  //=============================================================================

  QWidget *UIComponents::createUI()
  {
    // Wrap content in a QFrame matching OBS's own native-dock pattern
    // (e.g. controlsFrame/scenesFrame), so the dock gets the same boxed,
    // padded look as native docks. The background/border is drawn
    // explicitly here (rather than relying on OBS's theme QSS, which
    // targets its own dock widgets by name and doesn't reliably paint on
    // a plain QWidget) using palette-relative colors so it still adapts
    // to light/dark OBS themes.
    QFrame *container = new QFrame();
    container->setObjectName("replayBufferProFrame");
    container->setFrameShape(QFrame::NoFrame);
    container->setStyleSheet(
      "QFrame#replayBufferProFrame {"
      "  background: palette(base);"
      "  border: 1px solid palette(mid);"
      "  border-top: none;"
      "  border-bottom-left-radius: 4px;"
      "  border-bottom-right-radius: 4px;"
      "}"
    );
    QVBoxLayout *mainLayout = new QVBoxLayout(container);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    // Add the configuration title as a subtitle
    QLabel* subtitle = new QLabel(obs_module_text("WidgetTitle"), container);
    subtitle->setStyleSheet("opacity: .75; font-size: 14px; font-weight: bold;");
    mainLayout->addWidget(subtitle);
    mainLayout->addSpacing(4);

    // Create horizontal layout for label and seconds input
    QHBoxLayout *headerLayout = new QHBoxLayout();

    // Buffer length label
    QLabel* label = new QLabel(obs_module_text("BufferLengthLabel"), container);
    headerLayout->addWidget(label);
    headerLayout->addStretch();
    
    // Buffer length seconds input box
    secondsEdit = new QSpinBox(container);
    secondsEdit->setFixedWidth(104);
    secondsEdit->setAlignment(Qt::AlignRight);
    secondsEdit->setRange(Config::MIN_BUFFER_LENGTH, Config::MAX_BUFFER_LENGTH);
    secondsEdit->setSuffix(" sec");
    secondsEdit->setCursor(Qt::PointingHandCursor);
    auto* secondsEditFilter = new BufferLengthEventFilter();
    secondsEdit->installEventFilter(secondsEditFilter);
    secondsEdit->setContentsMargins(2, 2, 2, 2);
    headerLayout->addWidget(secondsEdit);
    mainLayout->addLayout(headerLayout);
    mainLayout->addSpacing(12); // Space before save clip section

    // Save clip label + customize button
    QHBoxLayout *saveClipHeaderLayout = new QHBoxLayout();
    QLabel* saveClipLabel = new QLabel(obs_module_text("SaveClipLabel"), container);
    saveClipLabel->setStyleSheet("opacity: .75; font-size: 14px; font-weight: bold;");
    saveClipHeaderLayout->addWidget(saveClipLabel);
    saveClipHeaderLayout->addStretch();

    customizeSaveButtonsBtn = new QPushButton(obs_module_text("CustomizeButtons"), container);
    customizeSaveButtonsBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    if (onCustomizeSaveButtons)
    {
      QObject::connect(customizeSaveButtonsBtn, &QPushButton::clicked, onCustomizeSaveButtons);
    }

    saveClipHeaderLayout->addWidget(customizeSaveButtonsBtn);
    mainLayout->addLayout(saveClipHeaderLayout);
    mainLayout->addSpacing(8);  // Space after save clip label

    // Save clip buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    initSaveButtons(buttonLayout);
    mainLayout->addLayout(buttonLayout);

    mainLayout->addStretch();
    return container;
  }

  void UIComponents::initSaveButtons(QHBoxLayout *layout)
  {
    saveButtons.clear();

    if (saveButtonDurations.size() != Config::SAVE_BUTTON_COUNT)
    {
      saveButtonDurations = getDefaultSaveButtonDurations();
    }

    QGridLayout *gridLayout = new QGridLayout();
    gridLayout->setSpacing(5);

    const int buttonsPerRow = 3;

    for (size_t i = 0; i < Config::SAVE_BUTTON_COUNT; i++)
    {
      auto *button = new QPushButton();
      int duration = saveButtonDurations[i];
      button->setText(formatDurationLabel(duration));
      button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

      QObject::connect(button, &QPushButton::clicked, [this, index = i]()
                       { onSaveSegment(saveButtonDurations[index]); });

      int row = static_cast<int>(i) / buttonsPerRow;
      int col = static_cast<int>(i) % buttonsPerRow;
      gridLayout->addWidget(button, row, col);

      saveButtons.push_back(button);
    }

    saveFullBufferBtn = new QPushButton(obs_module_text("SaveFull"));
    saveFullBufferBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    int lastRow = static_cast<int>(saveButtons.size() - 1) / buttonsPerRow + 1;
    gridLayout->addWidget(saveFullBufferBtn, lastRow, 0, 1, buttonsPerRow);

    QObject::connect(saveFullBufferBtn, &QPushButton::clicked, onSaveFullBuffer);

    layout->addLayout(gridLayout);
  }

  //=============================================================================
  // UI STATE MANAGEMENT
  //=============================================================================

  void UIComponents::updateBufferLengthValue(int seconds)
  {
    secondsEdit->setValue(seconds);

    toggleSaveButtons(seconds);
  }

  void UIComponents::updateBufferLengthState(bool isActive)
  {
    isBufferActive = isActive;  // Store the active state
    secondsEdit->setEnabled(!isActive);
  }

  void UIComponents::toggleSaveButtons(int bufferLength)
  {
    if (saveButtons.size() != saveButtonDurations.size())
    {
      return;
    }

    for (size_t i = 0; i < Config::SAVE_BUTTON_COUNT; i++)
    {
      int duration = saveButtonDurations[i];
      saveButtons[i]->setEnabled(bufferLength >= duration);
    }
  }

  void UIComponents::setSaveButtonDurations(const std::vector<int> &durations)
  {
    saveButtonDurations = getDefaultSaveButtonDurations();
    size_t limit = std::min(durations.size(), saveButtonDurations.size());
    for (size_t i = 0; i < limit; i++)
    {
      saveButtonDurations[i] = durations[i];
    }

    updateSaveButtonLabels();
    toggleSaveButtons(secondsEdit ? secondsEdit->value() : Config::DEFAULT_BUFFER_LENGTH);
  }

  void UIComponents::updateSaveButtonLabels()
  {
    if (saveButtons.size() != saveButtonDurations.size())
    {
      return;
    }

    for (size_t i = 0; i < saveButtons.size(); i++)
    {
      saveButtons[i]->setText(formatDurationLabel(saveButtonDurations[i]));
    }
  }

  std::vector<int> UIComponents::getDefaultSaveButtonDurations() const
  {
    std::vector<int> defaults;
    defaults.reserve(Config::SAVE_BUTTON_COUNT);
    for (size_t i = 0; i < Config::SAVE_BUTTON_COUNT; i++)
    {
      defaults.push_back(Config::SAVE_BUTTONS[i]);
    }
    return defaults;
  }

} // namespace ReplayBufferPro

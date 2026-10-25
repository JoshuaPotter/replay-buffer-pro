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
#include <QToolBar>
#include <QAction>
#include <QIcon>

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
                             std::function<void()> openPropertiesCallback)
      : secondsEdit(nullptr),
        saveFullBufferBtn(nullptr),
        bufferLengthDebounceTimer(new QTimer(parent)),
        onSaveSegment(saveSegmentCallback),
        onSaveFullBuffer(saveFullBufferCallback),
        onOpenProperties(openPropertiesCallback)
  {
    if (!parent) {
        qWarning("UIComponents: parent widget cannot be null");
        return;
    }

    bufferLengthDebounceTimer->setSingleShot(true);
    bufferLengthDebounceTimer->setInterval(Config::BUFFER_LENGTH_DEBOUNCE_INTERVAL);
    saveButtonConfig = SaveButtonSettings::getDefaultButtons();
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
      "  border: 1px solid palette(button);"
      "  border-top: none;"
      "  border-bottom-left-radius: 4px;"
      "  border-bottom-right-radius: 4px;"
      "}"
    );
    // The padded content sits above a footer toolbar that spans the frame edge to edge
    QVBoxLayout *frameLayout = new QVBoxLayout(container);
    frameLayout->setContentsMargins(0, 0, 0, 0);
    frameLayout->setSpacing(0);
    QVBoxLayout *mainLayout = new QVBoxLayout();
    mainLayout->setContentsMargins(8, 8, 8, 8);
    frameLayout->addLayout(mainLayout);

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

    // Save clip label
    QLabel* saveClipLabel = new QLabel(obs_module_text("SaveClipLabel"), container);
    saveClipLabel->setStyleSheet("opacity: .75; font-size: 14px; font-weight: bold;");
    mainLayout->addWidget(saveClipLabel);
    mainLayout->addSpacing(8);  // Space after save clip label

    // Save clip buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    initSaveButtons(buttonLayout);
    mainLayout->addLayout(buttonLayout);

    mainLayout->addStretch();
    frameLayout->addWidget(createFooter(container));
    return container;
  }

  QToolBar *UIComponents::createFooter(QWidget *parent)
  {
    // Same construction as OBS's Audio Mixer footer: a spacer pushes the
    // properties button to the right, after a separator.
    QToolBar *toolbar = new QToolBar(parent);
    toolbar->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    toolbar->setIconSize(QSize(16, 16));
    toolbar->setFloatable(false);
    toolbar->setMovable(false);

    QWidget *spacer = new QWidget(toolbar);
    spacer->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    toolbar->addWidget(spacer);
    toolbar->addSeparator();

    QAction *propertiesAction = new QAction(toolbar);
    propertiesAction->setText(obs_module_text("PropertiesTitle"));
    propertiesAction->setToolTip(obs_module_text("PropertiesTitle"));
    // OBS's own gear icon; the theme's icon-gear class below swaps in its variant
    propertiesAction->setIcon(QIcon(":/settings/images/settings/general.svg"));
    if (onOpenProperties)
    {
      QObject::connect(propertiesAction, &QAction::triggered, onOpenProperties);
    }
    toolbar->addAction(propertiesAction);

    // OBS sets the class on the action's widget, not the QAction (AudioMixer.cpp)
    if (QWidget *propertiesButton = toolbar->widgetForAction(propertiesAction))
    {
      propertiesButton->setProperty("class", "icon-gear");
    }

    return toolbar;
  }

  void UIComponents::initSaveButtons(QHBoxLayout *layout)
  {
    saveButtonGrid = new QGridLayout();
    saveButtonGrid->setSpacing(5);

    saveFullBufferBtn = new QPushButton(obs_module_text("SaveFull"));
    saveFullBufferBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QObject::connect(saveFullBufferBtn, &QPushButton::clicked, onSaveFullBuffer);

    rebuildSaveButtons();
    layout->addLayout(saveButtonGrid);
  }

  void UIComponents::rebuildSaveButtons()
  {
    for (auto *button : saveButtons)
    {
      saveButtonGrid->removeWidget(button);
      delete button;
    }
    saveButtons.clear();
    saveButtonGrid->removeWidget(saveFullBufferBtn);

    const int buttonsPerRow = 3;

    for (size_t i = 0; i < saveButtonConfig.size(); i++)
    {
      const SaveButton &config = saveButtonConfig[i];
      auto *button = new QPushButton();
      button->setText(formatButtonLabel(config.seconds, config.label));
      if (!config.label.empty())
      {
        button->setToolTip(formatDurationLabel(config.seconds));
      }
      button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

      QObject::connect(button, &QPushButton::clicked, [this, index = i]()
                       { onSaveSegment(saveButtonConfig[index].seconds); });

      int row = static_cast<int>(i) / buttonsPerRow;
      int col = static_cast<int>(i) % buttonsPerRow;
      saveButtonGrid->addWidget(button, row, col);

      saveButtons.push_back(button);
    }

    int lastRow = static_cast<int>(saveButtons.size() + buttonsPerRow - 1) / buttonsPerRow;
    saveButtonGrid->addWidget(saveFullBufferBtn, lastRow, 0, 1, buttonsPerRow);
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
    if (saveButtons.size() != saveButtonConfig.size())
    {
      return;
    }

    for (size_t i = 0; i < saveButtons.size(); i++)
    {
      saveButtons[i]->setEnabled(bufferLength >= saveButtonConfig[i].seconds);
    }
  }

  void UIComponents::setSaveButtons(const std::vector<SaveButton> &buttons)
  {
    saveButtonConfig = buttons.empty() ? SaveButtonSettings::getDefaultButtons() : buttons;

    if (saveButtonGrid)
    {
      rebuildSaveButtons();
    }
    toggleSaveButtons(secondsEdit ? secondsEdit->value() : Config::DEFAULT_BUFFER_LENGTH);
  }

} // namespace ReplayBufferPro

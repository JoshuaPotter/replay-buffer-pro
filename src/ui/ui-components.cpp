/**
 * @file ui-components.cpp
 * @brief Implementation of UI components for the Replay Buffer Pro plugin
 */

#include "ui/ui-components.hpp"
#include "config/config.hpp"

// OBS includes
#include <obs-module.h>

// Qt includes
#include <QObject>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QPushButton>
#include <QGridLayout>
#include <QSizePolicy>
#include <QTimer>
#include <QFrame>

namespace ReplayBufferPro
{
  //=============================================================================
  // CONSTRUCTORS & DESTRUCTOR
  //=============================================================================

  UIComponents::UIComponents(QWidget *parent,
                             std::function<void(int)> saveSegmentCallback,
                             std::function<void()> saveFullBufferCallback)
      : slider(nullptr),
        secondsEdit(nullptr),
        saveFullBufferBtn(nullptr),
        sliderDebounceTimer(new QTimer(parent)),
        onSaveSegment(saveSegmentCallback),
        onSaveFullBuffer(saveFullBufferCallback)
  {
    if (!parent) {
        qWarning("UIComponents: parent widget cannot be null");
        return;
    }
    
    sliderDebounceTimer->setSingleShot(true);
    sliderDebounceTimer->setInterval(Config::SLIDER_DEBOUNCE_INTERVAL);
  }

  //=============================================================================
  // UI CREATION
  //=============================================================================

  QWidget *UIComponents::createUI()
  {
    QWidget *container = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(container);

    // Add the configuration title as a subtitle
    QLabel* subtitle = new QLabel(obs_module_text("WidgetTitle"), container);
    subtitle->setStyleSheet("opacity: .75; font-size: 14px; font-weight: bold;");
    mainLayout->addWidget(subtitle);
    mainLayout->addSpacing(4);

    // Create horizontal layout for label and seconds input
    QHBoxLayout *headerLayout = new QHBoxLayout();
    QLabel* label = new QLabel(obs_module_text("BufferLengthLabel"), container);
    headerLayout->addWidget(label);
    headerLayout->addStretch();
    
    secondsEdit = new QSpinBox(container);
    secondsEdit->setFixedWidth(70);
    secondsEdit->setAlignment(Qt::AlignRight);
    secondsEdit->setRange(Config::MIN_BUFFER_LENGTH, Config::MAX_BUFFER_LENGTH);
    secondsEdit->setSuffix(" sec");
    secondsEdit->setButtonSymbols(QAbstractSpinBox::NoButtons);
    secondsEdit->setCursor(Qt::PointingHandCursor);
    headerLayout->addWidget(secondsEdit);
    
    mainLayout->addLayout(headerLayout);
    mainLayout->addSpacing(-4);  // Reduce space between header and slider

    // Style the slider directly
    slider = new QSlider(Qt::Horizontal, container);
    slider->setRange(Config::MIN_BUFFER_LENGTH, Config::MAX_BUFFER_LENGTH);
    mainLayout->addWidget(slider);

    mainLayout->addSpacing(12);  // Space before divider

    // Add horizontal line divider
    QFrame* line = new QFrame(container);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line);
    
    mainLayout->addSpacing(12);  // Make spacing exactly equal to pre-divider spacing

    QLabel* saveClipLabel = new QLabel(obs_module_text("SaveClipLabel"), container);  
    saveClipLabel->setStyleSheet("opacity: .75; font-size: 14px; font-weight: bold;");
    mainLayout->addWidget(saveClipLabel);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    initSaveButtons(buttonLayout);
    mainLayout->addLayout(buttonLayout);

    mainLayout->addStretch();
    return container;
  }

  void UIComponents::initSaveButtons(QHBoxLayout *layout)
  {
    saveButtons.clear();

    QGridLayout *gridLayout = new QGridLayout();
    gridLayout->setSpacing(5);

    const int buttonsPerRow = 3;

    for (size_t i = 0; i < Config::SAVE_BUTTON_COUNT; i++)
    {
      auto *button = new QPushButton();
      const auto &btn = Config::SAVE_BUTTONS[i];
      button->setText(obs_module_text(btn.text));
      button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

      QObject::connect(button, &QPushButton::clicked, [this, duration = btn.duration]()
                       { onSaveSegment(duration); });

      int row = i / buttonsPerRow;
      int col = i % buttonsPerRow;
      gridLayout->addWidget(button, row, col);

      saveButtons.push_back(button);
    }

    saveFullBufferBtn = new QPushButton(obs_module_text("SaveFull"));
    saveFullBufferBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    int lastRow = (saveButtons.size() - 1) / buttonsPerRow + 1;
    gridLayout->addWidget(saveFullBufferBtn, lastRow, 0, 1, buttonsPerRow);

    QObject::connect(saveFullBufferBtn, &QPushButton::clicked, onSaveFullBuffer);

    layout->addLayout(gridLayout);
  }

  //=============================================================================
  // UI STATE MANAGEMENT
  //=============================================================================

  void UIComponents::updateBufferLengthValue(int seconds)
  {
    slider->setValue(seconds);
    secondsEdit->setValue(seconds);

    toggleSaveButtons(seconds);
  }

  void UIComponents::updateBufferLengthState(bool isActive)
  {
    slider->setEnabled(!isActive);
    secondsEdit->setEnabled(!isActive);
  }

  void UIComponents::toggleSaveButtons(int bufferLength)
  {
    for (size_t i = 0; i < Config::SAVE_BUTTON_COUNT; i++)
    {
      saveButtons[i]->setEnabled(bufferLength >= Config::SAVE_BUTTONS[i].duration);
    }
  }
} // namespace ReplayBufferPro
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
#include <QLineEdit>
#include <QPushButton>
#include <QGridLayout>
#include <QSizePolicy>
#include <QTimer>

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

    mainLayout->addWidget(new QLabel(obs_module_text("BufferLength"), container));

    QHBoxLayout *sliderLayout = new QHBoxLayout();
    sliderLayout->setAlignment(Qt::AlignTop);

    slider = new QSlider(Qt::Horizontal, container);
    slider->setRange(Config::MIN_BUFFER_LENGTH, Config::MAX_BUFFER_LENGTH);

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
    return container;
  }

  void UIComponents::initSaveButtons(QHBoxLayout *layout)
  {
    saveButtons.clear();

    QGridLayout *gridLayout = new QGridLayout();
    gridLayout->setSpacing(5);

    const int buttonsPerRow = 4;

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
    secondsEdit->setText(QString::number(seconds));

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
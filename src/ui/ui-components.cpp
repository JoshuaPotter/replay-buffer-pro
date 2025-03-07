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
#include <QResizeEvent>
#include <QMessageBox>

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
                             std::function<void()> saveFullBufferCallback)
      : slider(nullptr),
        secondsEdit(nullptr),
        saveFullBufferBtn(nullptr),
        sliderDebounceTimer(new QTimer(parent)),
        tickWidget(nullptr),
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

    // Buffer length label
    QLabel* label = new QLabel(obs_module_text("BufferLengthLabel"), container);
    headerLayout->addWidget(label);
    headerLayout->addStretch();
    
    // Buffer length seconds input box
    secondsEdit = new QSpinBox(container);
    secondsEdit->setFixedWidth(80);
    secondsEdit->setAlignment(Qt::AlignRight);
    secondsEdit->setRange(Config::MIN_BUFFER_LENGTH, Config::MAX_BUFFER_LENGTH);
    secondsEdit->setSuffix(" sec");
    secondsEdit->setButtonSymbols(QAbstractSpinBox::NoButtons);
    secondsEdit->setCursor(Qt::PointingHandCursor);
    secondsEdit->installEventFilter(new BufferLengthEventFilter());
    secondsEdit->setContentsMargins(2, 2, 2, 2);
    headerLayout->addWidget(secondsEdit);
    mainLayout->addLayout(headerLayout);
    mainLayout->addSpacing(4);

    // Buffer length slider
    slider = new QSlider(Qt::Horizontal, container);
    slider->setRange(Config::MIN_BUFFER_LENGTH, Config::MAX_BUFFER_LENGTH);
    slider->installEventFilter(new BufferLengthEventFilter());
    
    // Create custom tick label widget
    tickWidget = new TickLabelWidget(container, &isBufferActive);
    tickWidget->setFixedHeight(20);
    tickWidget->setValueCallback([this](int seconds) {
        updateBufferLengthValue(seconds);
    });
    
    mainLayout->addWidget(slider);
    mainLayout->setSpacing(0);  // Reduce spacing between slider & ticks
    mainLayout->addWidget(tickWidget);

    mainLayout->addSpacing(18);  // Space before divider

    // Add horizontal line divider
    QFrame* line = new QFrame(container);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line);
    
    mainLayout->addSpacing(24); // Space after divider

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
    isBufferActive = isActive;  // Store the active state
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

  //=============================================================================
  // TickLabelWidget Implementation
  //=============================================================================

  TickLabelWidget::TickLabelWidget(QWidget* parent, bool* isBufferActive) 
      : QWidget(parent), isBufferActive(isBufferActive) {
      // Define all possible tick marks (in seconds) in order of priority
      allTicks = {
          // Primary tick marks - always shown when space permits
          {21600, "6h"},   // 6 hours - maximum buffer length
          {300, "5m"},     // 5 minutes - minimum meaningful segment
          
          // Hour markers - shown second if space allows
          {3600, "1h"},    // 1 hour
          {7200, "2h"},    // 2 hours  
          {10800, "3h"},   // 3 hours
          {14400, "4h"},   // 4 hours
          {18000, "5h"},   // 5 hours
          
          // Half-hour markers - lowest priority, shown last
          {1800, "30m"},   // 30 minutes
          {5400, "1.5h"},  // 1.5 hours
          {9000, "2.5h"},  // 2.5 hours
          {12600, "3.5h"}, // 3.5 hours
          {16200, "4.5h"}, // 4.5 hours
          {19800, "5.5h"}, // 5.5 hours

          // Minute markers - shown third if space allows
          {2700, "45m"},   // 45 minutes
          {900, "15m"},    // 15 minutes
          {600, "10m"},    // 10 minutes
      };

      // Create all labels (initially hidden)
      for (const auto& tick : allTicks) {
          QLabel* label = new QLabel(tick.second, this);
          label->setAlignment(Qt::AlignCenter);
          label->adjustSize();
          label->hide();
          label->setCursor(Qt::PointingHandCursor); // Show hand cursor on hover
          label->setStyleSheet("QLabel:hover { color: #999999; }"); // Optional: visual feedback on hover
          
          // Install event filter to handle clicks
          label->installEventFilter(this);
          labels.push_back(label);
      }
  }

  bool TickLabelWidget::eventFilter(QObject* obj, QEvent* event) {
      if (event->type() == QEvent::MouseButtonRelease) {
          // Show error dialog if buffer is active
          if (isBufferActive && *isBufferActive) {
              QMessageBox::warning(
                  this,
                  obs_module_text("Warning"),
                  obs_module_text("ReplayBufferActive"),
                  QMessageBox::Ok
              );
              return QWidget::eventFilter(obj, event);
          }

          if (auto* label = qobject_cast<QLabel*>(obj)) {
              for (size_t i = 0; i < labels.size(); i++) {
                  if (labels[i] == label && onValueChanged) {
                      onValueChanged(allTicks[i].first);
                      break;
                  }
              }
          }
      }
      return QWidget::eventFilter(obj, event);
  }

  void TickLabelWidget::resizeEvent(QResizeEvent* event) {
      QWidget::resizeEvent(event);
      updateVisibleTicks();
      updateTickPositions();
  }

  void TickLabelWidget::setValueCallback(std::function<void(int)> callback) {
      onValueChanged = callback;
  }

  void TickLabelWidget::updateVisibleTicks() {
      const int totalWidth = width();
      const int minSpaceBetweenLabels = 50; // Minimum pixels between labels
      
      // Hide all labels first
      for (auto* label : labels) {
          label->hide();
      }

      // Always try to show highest priority ticks (6h and 5m) first
      if (totalWidth >= minSpaceBetweenLabels * 2) {
          labels[0]->show(); // 6h
          labels[1]->show(); // 5m

          // Create a list of visible labels to check spacing
          std::vector<QLabel*> visibleLabels = {labels[0], labels[1]};

          // Try to add hour markers (indices 2-6)
          for (size_t i = 2; i <= 6; i++) {
              labels[i]->show();
              visibleLabels.push_back(labels[i]);
              
              // Sort by position
              std::sort(visibleLabels.begin(), visibleLabels.end(),
                  [this](QLabel* a, QLabel* b) {
                      return getTickPosition(a) < getTickPosition(b);
                  });

              // Check spacing
              bool hasEnoughSpace = true;
              for (size_t j = 1; j < visibleLabels.size(); j++) {
                  double pos1 = getTickPosition(visibleLabels[j-1]) * totalWidth;
                  double pos2 = getTickPosition(visibleLabels[j]) * totalWidth;
                  if (pos2 - pos1 < minSpaceBetweenLabels) {
                      hasEnoughSpace = false;
                      break;
                  }
              }

              if (!hasEnoughSpace) {
                  labels[i]->hide();
                  visibleLabels.pop_back();
              }
          }

          // Try to add minute markers (indices 7-9)
          for (size_t i = 7; i <= 9; i++) {
              labels[i]->show();
              visibleLabels.push_back(labels[i]);
              
              std::sort(visibleLabels.begin(), visibleLabels.end(),
                  [this](QLabel* a, QLabel* b) {
                      return getTickPosition(a) < getTickPosition(b);
                  });

              bool hasEnoughSpace = true;
              for (size_t j = 1; j < visibleLabels.size(); j++) {
                  double pos1 = getTickPosition(visibleLabels[j-1]) * totalWidth;
                  double pos2 = getTickPosition(visibleLabels[j]) * totalWidth;
                  if (pos2 - pos1 < minSpaceBetweenLabels) {
                      hasEnoughSpace = false;
                      break;
                  }
              }

              if (!hasEnoughSpace) {
                  labels[i]->hide();
                  visibleLabels.pop_back();
              }
          }

          // Try to add half-hour markers (indices 10-15) last
          for (size_t i = 10; i < labels.size(); i++) {
              labels[i]->show();
              visibleLabels.push_back(labels[i]);
              
              std::sort(visibleLabels.begin(), visibleLabels.end(),
                  [this](QLabel* a, QLabel* b) {
                      return getTickPosition(a) < getTickPosition(b);
                  });

              bool hasEnoughSpace = true;
              for (size_t j = 1; j < visibleLabels.size(); j++) {
                  double pos1 = getTickPosition(visibleLabels[j-1]) * totalWidth;
                  double pos2 = getTickPosition(visibleLabels[j]) * totalWidth;
                  if (pos2 - pos1 < minSpaceBetweenLabels) {
                      hasEnoughSpace = false;
                      break;
                  }
              }

              if (!hasEnoughSpace) {
                  labels[i]->hide();
                  visibleLabels.pop_back();
              }
          }
      }
  }

  void TickLabelWidget::updateTickPositions() {
      const int totalWidth = width();
      
      for (size_t i = 0; i < labels.size(); i++) {
          QLabel* label = labels[i];
          if (!label->isVisible()) continue;

          double position = static_cast<double>(allTicks[i].first - Config::MIN_BUFFER_LENGTH) / 
                          (Config::MAX_BUFFER_LENGTH - Config::MIN_BUFFER_LENGTH);
          
          int labelWidth = label->sizeHint().width();
          int x = static_cast<int>(position * totalWidth);
          
          // Special handling for 6h mark (first tick) and 5m mark (second tick)
          if (i == 0) { // 6h mark
              x = totalWidth - labelWidth; // Always align to far right
          } else if (i == 1) { // 5m mark
              x = 0; // Always align to far left
          } else {
              // Center all other labels
              x = std::max(0, std::min(totalWidth - labelWidth, x - labelWidth / 2));
          }
          
          label->move(x, 0);
      }
  }

  double TickLabelWidget::getTickPosition(QLabel* label) {
      for (size_t i = 0; i < labels.size(); i++) {
          if (labels[i] == label) {
              return static_cast<double>(allTicks[i].first - Config::MIN_BUFFER_LENGTH) / 
                     (Config::MAX_BUFFER_LENGTH - Config::MIN_BUFFER_LENGTH);
          }
      }
      return 0.0;
  }
} // namespace ReplayBufferPro
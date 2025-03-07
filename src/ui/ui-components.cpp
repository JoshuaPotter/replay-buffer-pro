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

    // Buffer length label
    QLabel* label = new QLabel(obs_module_text("BufferLengthLabel"), container);
    headerLayout->addWidget(label);
    headerLayout->addStretch();
    
    // Buffer length seconds input box
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

    // Buffer length slider
    slider = new QSlider(Qt::Horizontal, container);
    slider->setRange(Config::MIN_BUFFER_LENGTH, Config::MAX_BUFFER_LENGTH);
    
    // Create custom tick label widget
    class TickLabelWidget : public QWidget {
    public:
        TickLabelWidget(QWidget* parent = nullptr) : QWidget(parent) {
            // Define all possible tick marks (in seconds) in order of priority
            allTicks = {
                {21600, "6h"},   // Highest priority - always show if possible
                {300, "5m"},     // Highest priority - always show if possible
                {3600, "1h"},    // Secondary priority
                {7200, "2h"},    // Secondary priority
                {10800, "3h"},   // Lower priority
                {14400, "4h"},   // Lower priority
                {18000, "5h"},   // Lower priority
                {2700, "45m"},   // Lowest priority
                {1800, "30m"},   // Lowest priority
                {900, "15m"},    // Lowest priority
                {600, "10m"}     // Lowest priority
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

        // Signal to emit when a tick is clicked
        void setValueCallback(std::function<void(int)> callback) {
            onValueChanged = callback;
        }

    protected:
        bool eventFilter(QObject* obj, QEvent* event) override {
            if (event->type() == QEvent::MouseButtonRelease) {
                QLabel* label = qobject_cast<QLabel*>(obj);
                if (label) {
                    // Find the corresponding tick value
                    for (size_t i = 0; i < labels.size(); i++) {
                        if (labels[i] == label) {
                            if (onValueChanged) {
                                onValueChanged(allTicks[i].first);
                            }
                            break;
                        }
                    }
                }
            }
            return QWidget::eventFilter(obj, event);
        }

        void resizeEvent(QResizeEvent* event) override {
            QWidget::resizeEvent(event);
            updateVisibleTicks();
            updateTickPositions();
        }

    private:
        std::function<void(int)> onValueChanged;
        void updateVisibleTicks() {
            const int totalWidth = width();
            const int minSpaceBetweenLabels = 50; // Minimum pixels between labels
            
            // Hide all labels first
            for (auto* label : labels) {
                label->hide();
            }

            // Always try to show highest priority ticks (5m and 6h) first
            if (totalWidth >= minSpaceBetweenLabels * 2) {
                labels[0]->show(); // 6h
                labels[1]->show(); // 5m
            }

            // Try to add more labels in priority order
            for (size_t i = 2; i < allTicks.size(); i++) {
                labels[i]->show();
                
                // Check if we have enough space between all visible labels
                bool hasEnoughSpace = true;
                std::vector<QLabel*> visibleLabels;
                for (auto* label : labels) {
                    if (label->isVisible()) {
                        visibleLabels.push_back(label);
                    }
                }

                // Sort visible labels by their x position
                std::sort(visibleLabels.begin(), visibleLabels.end(),
                    [this](QLabel* a, QLabel* b) {
                        double posA = getTickPosition(a);
                        double posB = getTickPosition(b);
                        return posA < posB;
                    });

                // Check spacing between adjacent labels
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
                }
            }
        }

        double getTickPosition(QLabel* label) {
            for (size_t i = 0; i < labels.size(); i++) {
                if (labels[i] == label) {
                    return static_cast<double>(allTicks[i].first - Config::MIN_BUFFER_LENGTH) / 
                           (Config::MAX_BUFFER_LENGTH - Config::MIN_BUFFER_LENGTH);
                }
            }
            return 0.0;
        }

        void updateTickPositions() {
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

        std::vector<std::pair<int, QString>> allTicks;
        std::vector<QLabel*> labels;
    };

    // Create and add tick widget
    auto* tickWidget = new TickLabelWidget(container);
    tickWidget->setFixedHeight(20);
    tickWidget->setValueCallback([this](int seconds) {
        updateBufferLengthValue(seconds);
    });
    
    mainLayout->addWidget(slider);
    mainLayout->addWidget(tickWidget);

    mainLayout->addSpacing(12);  // Space before divider

    // Add horizontal line divider
    QFrame* line = new QFrame(container);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line);
    
    mainLayout->addSpacing(12);  // Make spacing exactly equal to pre-divider spacing

    // Save clip label
    QLabel* saveClipLabel = new QLabel(obs_module_text("SaveClipLabel"), container);  
    saveClipLabel->setStyleSheet("opacity: .75; font-size: 14px; font-weight: bold;");
    mainLayout->addWidget(saveClipLabel);

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
/**
 * @file properties-dialog.cpp
 * @brief Implementation of the Replay Buffer Pro properties window
 */

#include "ui/properties-dialog.hpp"
#include "ui/hotkey-edit.hpp"
#include "config/config.hpp"
#include "utils/duration-format.hpp"

// OBS includes
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>
#include <util/dstr.h>

// Qt includes
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFrame>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

// STL includes
#include <algorithm>

namespace ReplayBufferPro
{
  namespace
  {
    constexpr const char *kWebsiteUrl = "https://joshuapotter.github.io/replay-buffer-pro/";
    constexpr const char *kUsageUrl = "https://joshuapotter.github.io/replay-buffer-pro/usage/";
    constexpr const char *kChangelogUrl = "https://joshuapotter.github.io/replay-buffer-pro/changelog/";
    constexpr const char *kIssuesUrl = "https://github.com/JoshuaPotter/replay-buffer-pro/issues";
    constexpr int kMaxLabelLength = 40;
    constexpr int kMinimumWidth = 800;
    constexpr int kMinimumHeight = 360;
    constexpr int kDefaultWidth = 820;
    constexpr int kDefaultHeight = 600;

    QString link(const char *url, const char *textKey)
    {
      return QString("<a href=\"%1\">%2</a>")
        .arg(QString::fromUtf8(url), QString::fromUtf8(obs_module_text(textKey)).toHtmlEscaped());
    }

    QLabel *createMutedLabel(const char *text, QWidget *parent)
    {
      auto *label = new QLabel(QString::fromUtf8(text), parent);
      label->setWordWrap(true);
      label->setProperty("class", "text-muted"); // OBS theme class
      return label;
    }

    // A setting: its name with the description directly under it, and the
    // control on the right
    QHBoxLayout *createSettingRow(const char *name, const char *description, QWidget *input, QWidget *parent)
    {
      auto *nameLabel = new QLabel(QString::fromUtf8(name), parent);
      nameLabel->setBuddy(input);
      input->setAccessibleName(nameLabel->text());

      auto *text = new QVBoxLayout();
      text->setSpacing(2);
      text->addWidget(nameLabel);
      text->addWidget(createMutedLabel(description, parent));

      auto *row = new QHBoxLayout();
      row->addLayout(text, 1);
      row->addSpacing(16);
      row->addWidget(input, 0, Qt::AlignRight | Qt::AlignVCenter);
      return row;
    }

    // OBS turns hotkeys back on with OBSApp::UpdateHotkeyFocusSetting(), which
    // the frontend API does not export. This applies the same rule from the
    // same "Hotkey focus behavior" setting.
    void restoreHotkeyPress()
    {
      config_t *config = obs_frontend_get_user_config();
      const char *focusType = config ? config_get_string(config, "General", "HotkeyFocusType") : nullptr;
      bool inFocus = QGuiApplication::applicationState() == Qt::ApplicationActive;
      const char *disabledType = inFocus ? "DisableHotkeysInFocus" : "DisableHotkeysOutOfFocus";
      obs_hotkey_enable_background_press(!focusType || astrcmpi(focusType, disabledType) != 0);
    }
  } // namespace

  PropertiesDialog::PropertiesDialog(const std::vector<SaveButton> &buttons,
                                 const std::vector<obs_key_combination_t> &hotkeys,
                                 bool inMemoryClips, Page page, QWidget *parent)
      : QDialog(parent)
  {
    setWindowTitle(obs_module_text("PropertiesTitle"));
    // Opens at a fixed size and is freely resizable; a page scrolls when it is
    // taller than the window
    setMinimumSize(kMinimumWidth, kMinimumHeight);
    resize(kDefaultWidth, kDefaultHeight);

    pageList = new QListWidget(this);
    pageList->setFixedWidth(160);
    pageList->addItem(obs_module_text("PropertiesPageSaveButtons"));
    pageList->addItem(obs_module_text("PropertiesPageExperimental"));
    pageList->addItem(obs_module_text("PropertiesPageAbout"));

    pages = new QStackedWidget(this);
    addPage(createSaveButtonsPage(buttons, hotkeys));
    addPage(createExperimentalPage(inMemoryClips));
    addPage(createAboutPage());

    connect(pageList, &QListWidget::currentRowChanged, pages, &QStackedWidget::setCurrentIndex);
    pageList->setCurrentRow(static_cast<int>(page));

    auto *buttonBox = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply, this);
    buttonBox->button(QDialogButtonBox::Ok)->setText(obs_module_text("PropertiesOk"));
    buttonBox->button(QDialogButtonBox::Cancel)->setText(obs_module_text("PropertiesCancel"));
    buttonBox->button(QDialogButtonBox::Apply)->setText(obs_module_text("PropertiesApply"));
    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
      apply();
      accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &PropertiesDialog::apply);

    auto *contentLayout = new QHBoxLayout();
    contentLayout->addWidget(pageList);
    contentLayout->addWidget(pages, 1);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(contentLayout, 1);
    layout->addWidget(buttonBox);

    // OBS's Settings window disables hotkeys for as long as it is open
    obs_hotkey_enable_background_press(false);
  }

  PropertiesDialog::~PropertiesDialog()
  {
    restoreHotkeyPress();
  }

  void PropertiesDialog::addPage(QWidget *content)
  {
    auto *scrollArea = new QScrollArea(pages);
    scrollArea->setWidget(content);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    // Let the dialog's background show through, as it did before the scroll area
    scrollArea->viewport()->setAutoFillBackground(false);
    content->setAutoFillBackground(false);
    pages->addWidget(scrollArea);
  }

  void PropertiesDialog::restoreSize(int width, int height)
  {
    if (width <= 0 || height <= 0)
    {
      return;
    }

    // A size saved on a larger monitor still has to fit this one
    QSize size(std::max(width, kMinimumWidth), std::max(height, kMinimumHeight));
    if (QScreen *currentScreen = screen())
    {
      size = size.boundedTo(currentScreen->availableGeometry().size());
    }
    resize(size);
  }

  QSize PropertiesDialog::sizeToRemember() const
  {
    if (size() == QSize(kDefaultWidth, kDefaultHeight))
    {
      return QSize(0, 0);
    }
    return size();
  }

  QWidget *PropertiesDialog::createSaveButtonsPage(const std::vector<SaveButton> &buttons,
                                                 const std::vector<obs_key_combination_t> &hotkeys)
  {
    auto *page = new QWidget(this);

    buttonCountInput = new QSpinBox(page);
    buttonCountInput->setRange(1, static_cast<int>(Config::MAX_SAVE_BUTTON_COUNT));
    buttonCountInput->setValue(static_cast<int>(buttons.size()));
    buttonCountInput->setAlignment(Qt::AlignRight);
    // Sized to its digits so it doesn't stretch across the page on Windows and Linux
    buttonCountInput->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    QHBoxLayout *countLayout = createSettingRow(obs_module_text("PropertiesButtonCount"),
                                                obs_module_text("PropertiesButtonCountNote"), buttonCountInput, page);

    auto *countDivider = new QFrame(page);
    countDivider->setFrameShape(QFrame::HLine);
    countDivider->setFrameShadow(QFrame::Sunken);

    auto *grid = new QGridLayout();
    grid->setColumnStretch(2, 1);
    grid->setColumnStretch(3, 1);
    grid->addWidget(new QLabel(obs_module_text("PropertiesDuration"), page), 0, 1);
    grid->addWidget(new QLabel(obs_module_text("PropertiesLabel"), page), 0, 2);
    grid->addWidget(new QLabel(obs_module_text("PropertiesHotkey"), page), 0, 3);

    auto addHotkeyEdit = [&](size_t index, int row) {
      auto *hotkeyEdit = new HotkeyEdit(page);
      if (index < hotkeys.size())
      {
        hotkeyEdit->setCombination(hotkeys[index]);
      }
      grid->addWidget(hotkeyEdit, row, 3);
      hotkeyInputs.push_back(hotkeyEdit);

      auto *clearButton = new QPushButton(obs_module_text("PropertiesHotkeyClear"), page);
      connect(clearButton, &QPushButton::clicked, hotkeyEdit, &HotkeyEdit::clearCombination);
      grid->addWidget(clearButton, row, 4);
      return clearButton;
    };

    // Every slot gets a row; the button count shows or hides them
    for (size_t i = 0; i < Config::MAX_SAVE_BUTTON_COUNT; i++)
    {
      int row = static_cast<int>(i) + 1;
      bool inUse = i < buttons.size();
      SaveButton button = inUse ? buttons[i] : SaveButton{Config::SAVE_BUTTON_DEFAULTS[i], std::string()};

      QString nameText = QString::fromUtf8(obs_module_text("SaveClipButtonLabel")).arg(i + 1);
      auto *name = new QLabel(nameText, page);
      grid->addWidget(name, row, 0);

      auto *spinBox = new QSpinBox(page);
      spinBox->setRange(1, Config::MAX_BUFFER_LENGTH);
      spinBox->setSuffix(" sec");
      spinBox->setValue(button.seconds);
      grid->addWidget(spinBox, row, 1);

      // An empty label shows the default "Last ..." text as its placeholder
      auto *labelEdit = new QLineEdit(QString::fromStdString(button.label), page);
      labelEdit->setMaxLength(kMaxLabelLength);
      labelEdit->setPlaceholderText(formatDurationLabel(spinBox->value()));
      connect(spinBox, QOverload<int>::of(&QSpinBox::valueChanged), labelEdit,
              [labelEdit](int seconds) { labelEdit->setPlaceholderText(formatDurationLabel(seconds)); });
      grid->addWidget(labelEdit, row, 2);

      QPushButton *clear = addHotkeyEdit(i, row);
      buttonRows.push_back({name, spinBox, labelEdit, clear});
    }

    // Save Replay Buffer's hotkey is a setting row of its own: the field and its
    // Clear button side by side on the right
    auto *fullBufferHotkey = new HotkeyEdit(page);
    fullBufferHotkey->setMinimumWidth(200);
    if (FULL_BUFFER_HOTKEY < hotkeys.size())
    {
      fullBufferHotkey->setCombination(hotkeys[FULL_BUFFER_HOTKEY]);
    }
    hotkeyInputs.push_back(fullBufferHotkey);

    auto *fullBufferClear = new QPushButton(obs_module_text("PropertiesHotkeyClear"), page);
    connect(fullBufferClear, &QPushButton::clicked, fullBufferHotkey, &HotkeyEdit::clearCombination);

    auto *fullBufferInput = new QWidget(page);
    auto *fullBufferInputLayout = new QHBoxLayout(fullBufferInput);
    fullBufferInputLayout->setContentsMargins(0, 0, 0, 0);
    fullBufferInputLayout->addWidget(fullBufferHotkey);
    fullBufferInputLayout->addWidget(fullBufferClear);

    QHBoxLayout *fullBufferLayout = createSettingRow(obs_module_text("PropertiesFullBufferHotkey"),
                                                     obs_module_text("PropertiesFullBufferHotkeyNote"),
                                                     fullBufferInput, page);

    auto *fullBufferDivider = new QFrame(page);
    fullBufferDivider->setFrameShape(QFrame::HLine);
    fullBufferDivider->setFrameShadow(QFrame::Sunken);

    connect(buttonCountInput, QOverload<int>::of(&QSpinBox::valueChanged), this, &PropertiesDialog::showButtonRows);
    showButtonRows(buttonCountInput->value());

    auto *layout = new QVBoxLayout(page);
    layout->addLayout(countLayout);
    layout->addSpacing(4);
    layout->addWidget(countDivider);
    layout->addSpacing(4);
    layout->addLayout(grid);
    layout->addWidget(createMutedLabel(obs_module_text("PropertiesHotkeyNote"), page));
    layout->addSpacing(4);
    layout->addWidget(fullBufferDivider);
    layout->addSpacing(4);
    layout->addLayout(fullBufferLayout);
    layout->addStretch();
    return page;
  }

  void PropertiesDialog::showButtonRows(int count)
  {
    for (size_t i = 0; i < buttonRows.size(); i++)
    {
      bool visible = static_cast<int>(i) < count;
      const ButtonRow &row = buttonRows[i];
      row.name->setVisible(visible);
      row.duration->setVisible(visible);
      row.label->setVisible(visible);
      hotkeyInputs[i]->setVisible(visible);
      row.clear->setVisible(visible);
    }
  }

  QWidget *PropertiesDialog::createExperimentalPage(bool inMemoryClips)
  {
    auto *page = new QWidget(this);

    // The setting row's name is the checkbox's label, so the box itself has no text
    inMemoryClipsCheckBox = new QCheckBox(page);
    inMemoryClipsCheckBox->setChecked(inMemoryClips);

    auto *layout = new QVBoxLayout(page);
    layout->addLayout(createSettingRow(obs_module_text("InMemoryClips"), obs_module_text("InMemoryClipsTooltip"),
                                       inMemoryClipsCheckBox, page));
    layout->addStretch();
    return page;
  }

  QWidget *PropertiesDialog::createAboutPage()
  {
    auto *page = new QWidget(this);

    auto *title = new QLabel(obs_module_text("ReplayBufferPro"), page);
    title->setProperty("class", "text-large"); // OBS theme class

    auto *version = new QLabel(QString::fromUtf8(obs_module_text("PropertiesAboutVersion")).arg(PLUGIN_VERSION), page);

    QStringList links = {
      link(kUsageUrl, "PropertiesAboutUsage"),
      link(kChangelogUrl, "PropertiesAboutChangelog"),
      link(kWebsiteUrl, "PropertiesAboutWebsite"),
      link(kIssuesUrl, "PropertiesAboutIssues"),
    };
    auto *linkLabel = new QLabel(links.join("<br>"), page);
    linkLabel->setTextFormat(Qt::RichText);
    linkLabel->setOpenExternalLinks(true);

    auto *layout = new QVBoxLayout(page);
    layout->addWidget(title);
    layout->addWidget(version);
    layout->addSpacing(8);
    layout->addWidget(linkLabel);
    layout->addSpacing(8);
    layout->addWidget(createMutedLabel(obs_module_text("PropertiesAboutLogTip"), page));
    layout->addStretch();
    return page;
  }

  std::vector<SaveButton> PropertiesDialog::buttons() const
  {
    std::vector<SaveButton> values;
    int count = buttonCountInput->value();
    for (int i = 0; i < count; i++)
    {
      const ButtonRow &row = buttonRows[static_cast<size_t>(i)];
      values.push_back({row.duration->value(), row.label->text().trimmed().toStdString()});
    }
    return values;
  }

  bool PropertiesDialog::inMemoryClips() const
  {
    return inMemoryClipsCheckBox->isChecked();
  }

  std::vector<size_t> PropertiesDialog::changedHotkeys() const
  {
    std::vector<size_t> changed;
    size_t count = static_cast<size_t>(buttonCountInput->value());
    for (size_t i = 0; i < hotkeyInputs.size(); i++)
    {
      bool inUse = i < count || i == FULL_BUFFER_HOTKEY;
      if (inUse && hotkeyInputs[i]->isChanged())
      {
        changed.push_back(i);
      }
    }
    return changed;
  }

  obs_key_combination_t PropertiesDialog::hotkey(size_t index) const
  {
    return hotkeyInputs[index]->combination();
  }

  void PropertiesDialog::apply()
  {
    // Only the hotkeys just applied; an edit on a hidden row stays pending
    // until its row is shown and applied
    std::vector<size_t> applying = changedHotkeys();
    emit applied();
    for (size_t index : applying)
    {
      hotkeyInputs[index]->markApplied();
    }
  }

} // namespace ReplayBufferPro

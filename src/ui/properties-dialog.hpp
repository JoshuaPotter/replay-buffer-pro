/**
 * @file properties-dialog.hpp
 * @brief Replay Buffer Pro properties window
 * @author Joshua Potter
 * @copyright GPL v2 or later
 *
 * Every plugin option lives on a page of this window. New options get a page
 * (or a row on an existing one) here rather than a dialog of their own.
 */

#pragma once

// OBS includes
#include <obs.h>

// Qt includes
#include <QDialog>

// STL includes
#include <vector>

// Local includes
#include "config/config.hpp"
#include "managers/save-button-settings.hpp"

class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QWidget;

namespace ReplayBufferPro
{
  class HotkeyEdit;

  /**
   * @brief Properties window with a page list on the left, styled after OBS Settings
   *
   * OK and Apply emit applied(); the owner reads the getters and stores the
   * values. Cancel closes without applying anything since the last Apply.
   * OBS's background hotkeys are paused while the window is open, as OBS
   * does for its own Settings window, so pressing a bound key to rebind it
   * does not save a clip.
   */
  class PropertiesDialog : public QDialog
  {
    Q_OBJECT

  public:
    enum class Page
    {
      SaveButtons,
      Experimental,
      About
    };

    /**
     * @brief Hotkey index of the Save Replay Buffer row (same as HotkeyManager::FULL_BUFFER)
     */
    static constexpr size_t FULL_BUFFER_HOTKEY = Config::MAX_SAVE_BUTTON_COUNT;

    /**
     * @param buttons Current save buttons
     * @param hotkeys Current hotkey of every save button slot, then of Save Replay Buffer
     *                (MAX_SAVE_BUTTON_COUNT + 1 entries; an empty combination if unbound)
     * @param inMemoryClips Whether in-memory clips are on
     * @param page Page to open on
     * @param parent Parent widget
     */
    PropertiesDialog(const std::vector<SaveButton> &buttons,
                   const std::vector<obs_key_combination_t> &hotkeys,
                   bool inMemoryClips, Page page, QWidget *parent = nullptr);
    ~PropertiesDialog() override;

    /**
     * @brief The save buttons in use, with their durations and labels
     */
    std::vector<SaveButton> buttons() const;
    bool inMemoryClips() const;

    /**
     * @brief Hotkeys the user picked or cleared since the last Apply, for buttons in use
     *        and FULL_BUFFER_HOTKEY
     */
    std::vector<size_t> changedHotkeys() const;

    /**
     * @brief Hotkey shown for a save button or FULL_BUFFER_HOTKEY (an empty combination when cleared)
     */
    obs_key_combination_t hotkey(size_t index) const;

  signals:
    void applied();

  private:
    /**
     * @brief Widgets of one save button row, shown while its slot is in use
     */
    struct ButtonRow
    {
      QLabel *name;
      QSpinBox *duration;
      QLineEdit *label;
      QPushButton *clear;
    };

    QListWidget *pageList;
    QStackedWidget *pages;
    QSpinBox *buttonCountInput;
    std::vector<ButtonRow> buttonRows;
    std::vector<HotkeyEdit *> hotkeyInputs; ///< One per slot, then Save Replay Buffer
    QCheckBox *inMemoryClipsCheckBox;

    QWidget *createSaveButtonsPage(const std::vector<SaveButton> &buttons,
                                   const std::vector<obs_key_combination_t> &hotkeys);
    QWidget *createExperimentalPage(bool inMemoryClips);
    QWidget *createAboutPage();
    void showButtonRows(int count);

    /**
     * @brief Resizes the window to its content's height, keeping its width
     */
    void fitToContent();
    void apply();
  };

} // namespace ReplayBufferPro

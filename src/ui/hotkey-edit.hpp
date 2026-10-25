/**
 * @file hotkey-edit.hpp
 * @brief Line edit that captures one OBS key combination
 * @author Joshua Potter
 * @copyright GPL v2 or later
 *
 * Modelled on OBS's own OBSHotkeyEdit (frontend/settings/OBSHotkeyEdit.cpp),
 * which the frontend API does not export.
 */

#pragma once

// OBS includes
#include <obs.h>

// Qt includes
#include <QLineEdit>

class QKeyEvent;
class QMouseEvent;

namespace ReplayBufferPro
{
  /**
   * @brief Read-only line edit that records the key or mouse button pressed in it
   *
   * Keys are translated with obs_key_from_virtual_key(...) so the result
   * matches what OBS's Hotkeys page stores for the same press.
   */
  class HotkeyEdit : public QLineEdit
  {
    Q_OBJECT

  public:
    explicit HotkeyEdit(QWidget *parent = nullptr);

    obs_key_combination_t combination() const { return key; }

    /**
     * @brief Shows an existing binding without marking the field changed
     */
    void setCombination(obs_key_combination_t combination);

    /**
     * @brief Empties the field and marks it changed
     */
    void clearCombination();

    /**
     * @brief Whether the user picked or cleared a key since the last markApplied()
     */
    bool isChanged() const { return changed; }

    void markApplied() { changed = false; }

  protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;

  private:
    obs_key_combination_t key = {0, OBS_KEY_NONE};
    bool changed = false;

    void handleNewKey(obs_key_combination_t newKey);
    void renderKey();
  };

} // namespace ReplayBufferPro

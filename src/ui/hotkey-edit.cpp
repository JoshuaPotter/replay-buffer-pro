/**
 * @file hotkey-edit.cpp
 * @brief Implementation of the key combination capture field
 */

#include "ui/hotkey-edit.hpp"

// OBS includes
#include <obs-module.h>
#include <util/dstr.h>

// Qt includes
#include <QKeyEvent>
#include <QMouseEvent>
#include <QOperatingSystemVersion>
#include <QtAlgorithms>

namespace ReplayBufferPro
{
  namespace
  {
    // kVK_CapsLock; Qt reports no native virtual key for Caps Lock on macOS
    constexpr int kMacCapsLockVirtualKey = 57;

    bool isMacOS()
    {
      return QOperatingSystemVersion::currentType() == QOperatingSystemVersion::MacOS;
    }

    // Same mapping as OBS's TranslateQtKeyboardEventModifiers: on macOS Qt
    // reports Command as Control and Control as Meta.
    uint32_t translateModifiers(Qt::KeyboardModifiers mods)
    {
      uint32_t modifiers = INTERACT_NONE;
      if (mods.testFlag(Qt::ShiftModifier))
        modifiers |= INTERACT_SHIFT_KEY;
      if (mods.testFlag(Qt::AltModifier))
        modifiers |= INTERACT_ALT_KEY;
      if (mods.testFlag(Qt::ControlModifier))
        modifiers |= isMacOS() ? INTERACT_COMMAND_KEY : INTERACT_CONTROL_KEY;
      if (mods.testFlag(Qt::MetaModifier))
        modifiers |= isMacOS() ? INTERACT_CONTROL_KEY : INTERACT_COMMAND_KEY;
      return modifiers;
    }
  } // namespace

  HotkeyEdit::HotkeyEdit(QWidget *parent) : QLineEdit(parent)
  {
    setReadOnly(true);
    setAttribute(Qt::WA_InputMethodEnabled, false);
    setAttribute(Qt::WA_MacShowFocusRect, true);
    setPlaceholderText(obs_module_text("PropertiesHotkeyPlaceholder"));
  }

  void HotkeyEdit::setCombination(obs_key_combination_t combination)
  {
    key = combination;
    changed = false;
    renderKey();
  }

  void HotkeyEdit::clearCombination()
  {
    key = {0, OBS_KEY_NONE};
    changed = true;
    renderKey();
  }

  void HotkeyEdit::keyPressEvent(QKeyEvent *event)
  {
    if (event->isAutoRepeat())
      return;

    obs_key_combination_t newKey;
    switch (event->key())
    {
    case Qt::Key_Shift:
    case Qt::Key_Control:
    case Qt::Key_Alt:
    case Qt::Key_Meta:
      newKey.key = OBS_KEY_NONE;
      break;
    case Qt::Key_CapsLock:
      if (isMacOS())
      {
        newKey.key = obs_key_from_virtual_key(kMacCapsLockVirtualKey);
        break;
      }
      [[fallthrough]];
    default:
      newKey.key = obs_key_from_virtual_key(event->nativeVirtualKey());
    }

    newKey.modifiers = translateModifiers(event->modifiers());
    handleNewKey(newKey);
  }

  void HotkeyEdit::keyReleaseEvent(QKeyEvent *event)
  {
    // macOS sends Caps Lock as a press when it turns on and a release when it
    // turns off, so either one binds it.
    if (event->isAutoRepeat() || event->key() != Qt::Key_CapsLock || !isMacOS())
      return;

    obs_key_combination_t newKey;
    newKey.key = obs_key_from_virtual_key(kMacCapsLockVirtualKey);
    newKey.modifiers = translateModifiers(event->modifiers());
    handleNewKey(newKey);
  }

  void HotkeyEdit::mousePressEvent(QMouseEvent *event)
  {
    obs_key_combination_t newKey;
    Qt::MouseButton button = event->button();

    if (button == Qt::MiddleButton)
    {
      newKey.key = OBS_KEY_MOUSE3;
    }
    else if (button >= Qt::ExtraButton1 && button <= Qt::ExtraButton24)
    {
      // Qt::ExtraButtonN is 1 << (N + 2) and OBS_KEY_MOUSE4..27 are consecutive
      int extra = static_cast<int>(qCountTrailingZeroBits(static_cast<quint32>(button))) - 3;
      newKey.key = static_cast<obs_key_t>(OBS_KEY_MOUSE4 + extra);
    }
    else
    {
      // Left and right clicks focus the field
      QLineEdit::mousePressEvent(event);
      return;
    }

    newKey.modifiers = translateModifiers(event->modifiers());
    handleNewKey(newKey);
  }

  QVariant HotkeyEdit::inputMethodQuery(Qt::InputMethodQuery query) const
  {
    if (query == Qt::ImEnabled)
      return false;
    return QLineEdit::inputMethodQuery(query);
  }

  void HotkeyEdit::handleNewKey(obs_key_combination_t newKey)
  {
    if (obs_key_combination_is_empty(newKey) ||
        (newKey.key == key.key && newKey.modifiers == key.modifiers))
      return;

    key = newKey;
    changed = true;
    renderKey();
  }

  void HotkeyEdit::renderKey()
  {
    if (obs_key_combination_is_empty(key))
    {
      clear();
      return;
    }

    struct dstr str = {0};
    obs_key_combination_to_str(key, &str);
    setText(QString::fromUtf8(str.array ? str.array : ""));
    dstr_free(&str);
  }

} // namespace ReplayBufferPro

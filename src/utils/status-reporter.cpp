/**
 * @file status-reporter.cpp
 * @brief Implementation of OBS status bar messaging
 */

#include "utils/status-reporter.hpp"

// OBS includes
#include <obs-frontend-api.h>

// Qt includes
#include <QMainWindow>
#include <QMetaObject>
#include <QStatusBar>
#include <QTimer>

namespace ReplayBufferPro
{

  void StatusReporter::showMessage(const QString &message, int timeoutMs)
  {
    auto *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
    if (!mainWindow)
    {
      return;
    }

    // statusBar() creates the bar on demand and is not thread safe, so hop to
    // the main thread even when the caller might already be on it.
    QMetaObject::invokeMethod(
        mainWindow,
        [mainWindow, message, timeoutMs]()
        {
          if (QStatusBar *bar = mainWindow->statusBar())
          {
            bar->showMessage(message, timeoutMs);
          }
        },
        Qt::QueuedConnection);
  }

} // namespace ReplayBufferPro

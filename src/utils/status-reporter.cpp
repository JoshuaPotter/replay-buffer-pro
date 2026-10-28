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
            // OBS's OBSBasicStatusBar hides QStatusBar::showMessage with its own
            // slot, which writes to the label OBS displays. QStatusBar's message
            // area sits under OBS's stretched status widget and is never seen,
            // so call the slot by name to reach the most-derived one.
            QMetaObject::invokeMethod(bar, "showMessage", Qt::DirectConnection,
                                      Q_ARG(QString, message), Q_ARG(int, timeoutMs));
          }
        },
        Qt::QueuedConnection);
  }

} // namespace ReplayBufferPro

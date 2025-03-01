/**
 * @file dock_state_manager.cpp
 * @brief Implementation of dock state management for the Replay Buffer Pro plugin
 */

#include "dock-state-manager.hpp"
#include "config.hpp"
#include "logger.hpp"
#include "obs-utils.hpp"

// OBS includes
#include <util/platform.h>

// STL includes
#include <string>

namespace ReplayBufferPro
{
  //=============================================================================
  // CONSTRUCTORS & DESTRUCTOR
  //=============================================================================

  DockStateManager::DockStateManager(QDockWidget *dockWidget)
      : dockWidget(dockWidget)
  {
  }

  //=============================================================================
  // DOCK STATE MANAGEMENT
  //=============================================================================

  void DockStateManager::loadDockState(QMainWindow *mainWindow)
  {
    char *config_path = obs_module_config_path("dock_state.json");
    if (!config_path)
    {
      mainWindow->addDockWidget(Qt::LeftDockWidgetArea, dockWidget);
      return;
    }

    OBSDataRAII data(obs_data_create_from_json_file(config_path));
    bfree(config_path);

    if (!data.isValid())
    {
      mainWindow->addDockWidget(Qt::LeftDockWidgetArea, dockWidget);
      return;
    }

    Qt::DockWidgetArea area = static_cast<Qt::DockWidgetArea>(
        obs_data_get_int(data.get(), Config::DOCK_AREA_KEY));

    if (area != Qt::LeftDockWidgetArea &&
        area != Qt::RightDockWidgetArea &&
        area != Qt::TopDockWidgetArea &&
        area != Qt::BottomDockWidgetArea)
    {
      area = Qt::LeftDockWidgetArea;
    }

    mainWindow->addDockWidget(area, dockWidget);

    QByteArray geometry = QByteArray::fromBase64(
        obs_data_get_string(data.get(), Config::DOCK_GEOMETRY_KEY));
    if (!geometry.isEmpty())
    {
      dockWidget->restoreGeometry(geometry);
    }
  }

  void DockStateManager::saveDockState()
  {
    OBSDataRAII data(obs_data_create());
    if (!data.isValid())
      return;

    Qt::DockWidgetArea area = Qt::NoDockWidgetArea;
    if (QMainWindow *mainWindow = qobject_cast<QMainWindow *>(dockWidget->parent()))
    {
      area = mainWindow->dockWidgetArea(dockWidget);
    }
    obs_data_set_int(data.get(), Config::DOCK_AREA_KEY, static_cast<int>(area));

    if (dockWidget->isFloating())
    {
      QByteArray geometry = dockWidget->saveGeometry().toBase64();
      obs_data_set_string(data.get(), Config::DOCK_GEOMETRY_KEY, geometry.constData());
    }

    char *config_dir = obs_module_config_path("");
    if (!config_dir)
    {
      Logger::error("Failed to get config directory path");
      return;
    }

    if (os_mkdirs(config_dir) < 0)
    {
      Logger::error("Failed to create config directory: %s", config_dir);
      bfree(config_dir);
      return;
    }

    std::string config_path = std::string(config_dir) + "/" + Config::DOCK_STATE_FILENAME;
    bfree(config_dir);

    if (!obs_data_save_json_safe(data.get(), config_path.c_str(),
                                 Config::TEMP_FILE_SUFFIX, Config::BACKUP_FILE_SUFFIX))
    {
      Logger::error("Failed to save dock state to: %s", config_path.c_str());
    }
    else
    {
      Logger::info("Saved dock state to: %s", config_path.c_str());
    }
  }

} // namespace ReplayBufferPro
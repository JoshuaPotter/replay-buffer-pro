/**
 * @file update-checker.hpp
 * @brief Asks GitHub for the plugin's latest release
 * @author Joshua Potter
 * @copyright GPL v2 or later
 */

#pragma once

// Qt includes
#include <QObject>
#include <QString>

// STL includes
#include <atomic>
#include <thread>

namespace ReplayBufferPro
{
  /**
   * @brief Fetches the latest GitHub release on a worker thread and compares
   *        it with PLUGIN_VERSION
   *
   * Runs only when check() is called; nothing is fetched in the background.
   * finished() is emitted on the thread that owns the checker. Destroying the
   * checker aborts a request in flight and waits for the worker to stop.
   */
  class UpdateChecker : public QObject
  {
    Q_OBJECT

  public:
    struct Result
    {
      enum class Status
      {
        UpToDate,
        UpdateAvailable,
        Failed
      };

      Status status = Status::Failed;
      QString latestVersion; ///< Release tag without its leading "v"
      QString error;         ///< Short reason when status is Failed
    };

    explicit UpdateChecker(QObject *parent = nullptr);
    ~UpdateChecker() override;

    /**
     * @brief Starts a check; ignored while one is already running
     */
    void check();

  signals:
    void finished(const ReplayBufferPro::UpdateChecker::Result &result);

  private:
    std::thread worker;
    std::atomic<bool> cancelled{false};
    bool running = false;

    Result fetchLatest();
  };

} // namespace ReplayBufferPro

/**
 * @file update-checker.cpp
 * @brief Implementation of the latest-release check
 */

#include "utils/update-checker.hpp"
#include "utils/logger.hpp"
#include "utils/version-compare.hpp"

// Qt includes
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>

// Other includes
#include <curl/curl.h>

// STL includes
#include <string>

namespace ReplayBufferPro
{
  namespace
  {
    constexpr const char *kLatestReleaseApi =
      "https://api.github.com/repos/JoshuaPotter/replay-buffer-pro/releases/latest";
    constexpr long kConnectTimeoutSeconds = 10;
    constexpr long kTimeoutSeconds = 20;
    constexpr size_t kMaxResponseBytes = 1024 * 1024;

    size_t appendBody(char *data, size_t size, size_t count, void *userdata)
    {
      auto *body = static_cast<std::string *>(userdata);
      size_t bytes = size * count;
      if (body->size() + bytes > kMaxResponseBytes)
      {
        return 0; // Aborts the transfer
      }
      body->append(data, bytes);
      return bytes;
    }

    // Returning non-zero aborts the transfer, so closing the window doesn't
    // wait out the timeout
    int abortIfCancelled(void *clientp, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
    {
      return static_cast<std::atomic<bool> *>(clientp)->load() ? 1 : 0;
    }

    UpdateChecker::Result failed(const QString &error)
    {
      UpdateChecker::Result result;
      result.status = UpdateChecker::Result::Status::Failed;
      result.error = error;
      return result;
    }
  } // namespace

  UpdateChecker::UpdateChecker(QObject *parent) : QObject(parent) {}

  UpdateChecker::~UpdateChecker()
  {
    cancelled = true;
    if (worker.joinable())
    {
      worker.join();
    }
  }

  void UpdateChecker::check()
  {
    if (running)
    {
      return;
    }
    if (worker.joinable())
    {
      worker.join(); // The last check's thread, already done
    }

    running = true;
    cancelled = false;
    worker = std::thread([this]() {
      Result result = fetchLatest();
      // Dropped if the checker is destroyed before the event runs
      QMetaObject::invokeMethod(this, [this, result]() {
        running = false;
        emit finished(result);
      }, Qt::QueuedConnection);
    });
  }

  UpdateChecker::Result UpdateChecker::fetchLatest()
  {
    CURL *curl = curl_easy_init();
    if (!curl)
    {
      Logger::warning("UPDATE CHECK: failed reason=curl-init");
      return failed("curl-init");
    }

    std::string body;
    char errorBuffer[CURL_ERROR_SIZE] = {};
    std::string userAgent = std::string("replay-buffer-pro/") + PLUGIN_VERSION;

    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "Accept: application/vnd.github+json");

    curl_easy_setopt(curl, CURLOPT_URL, kLatestReleaseApi);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, kConnectTimeoutSeconds);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, kTimeoutSeconds);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, appendBody);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, abortIfCancelled);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &cancelled);

    CURLcode code = curl_easy_perform(curl);
    long httpStatus = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (cancelled)
    {
      return failed("cancelled");
    }
    if (code != CURLE_OK)
    {
      const char *reason = errorBuffer[0] ? errorBuffer : curl_easy_strerror(code);
      Logger::warning("UPDATE CHECK: failed reason=%s", reason);
      return failed(QString::fromUtf8(reason));
    }
    if (httpStatus != 200)
    {
      Logger::warning("UPDATE CHECK: failed reason=http-%ld", httpStatus);
      return failed(QString("HTTP %1").arg(httpStatus));
    }

    QJsonDocument document = QJsonDocument::fromJson(QByteArray::fromStdString(body));
    QString tag = document.object().value("tag_name").toString();
    std::optional<int> comparison = compareVersions(tag.toStdString(), PLUGIN_VERSION);
    if (!comparison)
    {
      Logger::warning("UPDATE CHECK: failed reason=unrecognized-tag tag=\"%s\"", tag.toUtf8().constData());
      return failed("unrecognized release");
    }

    Result result;
    result.latestVersion = tag.startsWith('v', Qt::CaseInsensitive) ? tag.mid(1) : tag;
    // A development build newer than the latest release is up to date
    result.status = *comparison > 0 ? Result::Status::UpdateAvailable : Result::Status::UpToDate;
    Logger::info("UPDATE CHECK: current=%s latest=%s update=%s", PLUGIN_VERSION,
                 result.latestVersion.toUtf8().constData(), *comparison > 0 ? "yes" : "no");
    return result;
  }

} // namespace ReplayBufferPro

// SPDX-License-Identifier: GPL-3.0-only
#include "ReleaseUpdateChecker.h"
#include "Version.h"
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>

namespace {
QString normalized(QString version)
{
    if (version.startsWith('v')) version.remove(0, 1);
    static const QRegularExpression syntax("^[0-9]+\\.[0-9]+\\.[0-9]+(?:-[0-9A-Za-z][0-9A-Za-z.-]*)?(?:\\+[0-9A-Za-z.-]+)?$");
    return syntax.match(version).hasMatch() ? version : QString();
}
constexpr qsizetype maxResponse = 4 * 1024 * 1024;
}

ReleaseUpdateChecker::ReleaseUpdateChecker(QNetworkAccessManager* network, const QString& settingsPath,
                                         const QString& repository, const QString& currentVersion)
    : m_network(network), m_settings(settingsPath, QSettings::IniFormat),
      m_repository(repository), m_currentVersion(currentVersion)
{
    m_timer.setSingleShot(true);
    m_timeout.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &ReleaseUpdateChecker::checkForUpdates);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        m_error = tr("The update check timed out. Try again later.");
        if (m_reply) m_reply->abort();
    });
    const auto cached = m_settings.value("release").toByteArray();
    if (m_settings.value("releaseRepository").toString() == m_repository && cached.size() <= maxResponse)
        m_release = selectRelease(cached, m_repository, m_currentVersion, getBetaAllowed(), nullptr);
    schedule();
}

ReleaseUpdateChecker::~ReleaseUpdateChecker()
{
    if (m_reply) {
        disconnect(m_reply, nullptr, this, nullptr);
        m_reply->abort();
        m_reply->deleteLater();
    }
}

LauncherRelease ReleaseUpdateChecker::selectRelease(const QByteArray& json, const QString& repository,
                                                     const QString& currentVersion, bool prereleases, QString* error)
{
    if (error) error->clear();
    QJsonParseError parse;
    const auto doc = QJsonDocument::fromJson(json, &parse);
    const auto current = normalized(currentVersion);
    if (parse.error != QJsonParseError::NoError || !doc.isArray() || current.isEmpty()) {
        if (error) *error = tr("The release information could not be read.");
        return {};
    }
    LauncherRelease result;
    auto newest = Version(current);
    for (const auto& value : doc.array()) {
        const auto obj = value.toObject();
        const auto tag = obj.value("tag_name").toString();
        const auto version = normalized(tag);
        const bool pre = obj.value("prerelease").toBool() || version.section('+', 0, 0).contains('-');
        if (obj.value("draft").toBool() || (!prereleases && pre) || version.isEmpty() || Version(version) <= newest)
            continue;
        newest = Version(version);
        result = {tag, obj.value("name").toString(), obj.value("body").toString(),
                  QUrl("https://github.com/" + repository + "/releases/tag/" + QString::fromUtf8(QUrl::toPercentEncoding(tag))), pre};
        if (result.name.isEmpty()) result.name = tag;
    }
    return result;
}

void ReleaseUpdateChecker::checkForUpdates()
{
    if (m_checking) return;
    m_timer.stop();
    m_error.clear();
    static const QRegularExpression repoSyntax("^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$");
    if (!m_network || !repoSyntax.match(m_repository).hasMatch() || normalized(m_currentVersion).isEmpty()) {
        m_error = tr("Update checks are unavailable in this build.");
        emit stateChanged();
        return;
    }
    m_checking = true;
    m_response.clear();
    QNetworkRequest request(QUrl("https://api.github.com/repos/" + m_repository + "/releases?per_page=100"));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    request.setRawHeader("User-Agent", "PXL-Cone-Launcher");
    request.setTransferTimeout(20000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::SameOriginRedirectPolicy);
    auto* reply = m_network->get(request);
    m_reply = reply;
    reply->setReadBufferSize(64 * 1024);
    connect(reply, &QIODevice::readyRead, this, [this, reply] {
        m_response += reply->readAll();
        if (m_response.size() > maxResponse) {
            m_error = tr("The release information is too large.");
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply] { finish(reply); });
    m_timeout.start(30000);
    emit canCheckForUpdatesChanged(false);
    emit stateChanged();
}

void ReleaseUpdateChecker::finish(QNetworkReply* reply)
{
    m_timeout.stop();
    m_response += reply->readAll();
    const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (m_error.isEmpty()) {
        if (status == 404) {
            m_error = tr("No published releases are available yet.");
        } else if (reply->error() != QNetworkReply::NoError || status != 200) {
            m_error = tr("Could not check for updates. Check your connection and try again.");
        } else if (m_response.size() > maxResponse) {
            m_error = tr("The release information is too large.");
        } else {
            const auto release = selectRelease(m_response, m_repository, m_currentVersion, getBetaAllowed(), &m_error);
            if (m_error.isEmpty()) {
                m_release = release;
                QJsonArray cached;
                if (release.isAvailable()) {
                    cached.append(QJsonObject{{"tag_name", release.tag}, {"name", release.name}, {"body", release.notes},
                                              {"prerelease", release.prerelease}});
                }
                m_settings.setValue("release", QJsonDocument(cached).toJson(QJsonDocument::Compact));
                m_settings.setValue("releaseRepository", m_repository);
            }
        }
    }
    m_response.clear();
    m_reply.clear();
    reply->deleteLater();
    m_checking = false;
    m_settings.setValue("lastCheck", QDateTime::currentDateTimeUtc());
    schedule();
    emit canCheckForUpdatesChanged(true);
    emit stateChanged();
}

bool ReleaseUpdateChecker::getAutomaticallyChecksForUpdates() { return m_settings.value("automatic", true).toBool(); }
double ReleaseUpdateChecker::getUpdateCheckInterval()
{
    const auto seconds = m_settings.value("interval", 86400).toDouble();
    return std::isfinite(seconds) ? std::clamp(seconds, 60.0, 604800.0) : 86400.0;
}
bool ReleaseUpdateChecker::getBetaAllowed() { return m_settings.value("prereleases", m_currentVersion.contains('-')).toBool(); }
void ReleaseUpdateChecker::setAutomaticallyChecksForUpdates(bool enabled) { m_settings.setValue("automatic", enabled); schedule(); }
void ReleaseUpdateChecker::setUpdateCheckInterval(double seconds) { m_settings.setValue("interval", seconds); schedule(); }
void ReleaseUpdateChecker::setBetaAllowed(bool enabled)
{
    m_settings.setValue("prereleases", enabled);
    if (!enabled && m_release.prerelease) {
        m_release = {};
        emit stateChanged();
    }
}
void ReleaseUpdateChecker::schedule()
{
    m_timer.stop();
    if (!getAutomaticallyChecksForUpdates() || m_checking) return;
    const auto last = m_settings.value("lastCheck").toDateTime();
    const auto remaining = last.isValid() ? getUpdateCheckInterval() - last.secsTo(QDateTime::currentDateTimeUtc()) : 10.0;
    m_timer.start(int(std::clamp(remaining, 10.0, 604800.0) * 1000));
}

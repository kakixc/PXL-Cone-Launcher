// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "ExternalUpdater.h"
#include <QPointer>
#include <QSettings>
#include <QTimer>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

struct LauncherRelease {
    QString tag, name, notes;
    QUrl url;
    bool prerelease = false;
    bool isAvailable() const { return !tag.isEmpty(); }
};

// Notification adapter; package installation remains a separate, explicit action.
class ReleaseUpdateChecker final : public ExternalUpdater {
    Q_OBJECT
public:
    ReleaseUpdateChecker(QNetworkAccessManager* network, const QString& settingsPath,
                         const QString& repository, const QString& currentVersion);
    ~ReleaseUpdateChecker() override;
    void checkForUpdates() override;
    bool getAutomaticallyChecksForUpdates() override;
    double getUpdateCheckInterval() override;
    bool getBetaAllowed() override;
    void setAutomaticallyChecksForUpdates(bool enabled) override;
    void setUpdateCheckInterval(double seconds) override;
    void setBetaAllowed(bool enabled) override;
    bool checking() const { return m_checking; }
    QString error() const { return m_error; }
    QString currentVersion() const { return m_currentVersion; }
    QString currentVersionLabel() const { return m_currentVersion == "0.0.0-dev" ? tr("Development preview") : m_currentVersion; }
    LauncherRelease release() const { return m_release; }
    static LauncherRelease selectRelease(const QByteArray& json, const QString& repository,
                                         const QString& currentVersion, bool prereleases, QString* error);
signals:
    void stateChanged();
private:
    void schedule();
    void finish(QNetworkReply* reply);
    QPointer<QNetworkAccessManager> m_network;
    QPointer<QNetworkReply> m_reply;
    QSettings m_settings;
    QString m_repository, m_currentVersion, m_error;
    LauncherRelease m_release;
    QTimer m_timer, m_timeout;
    QByteArray m_response;
    bool m_checking = false;
};

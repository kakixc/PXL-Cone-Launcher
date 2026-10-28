// SPDX-License-Identifier: GPL-3.0-only
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPushButton>
#include <QTextBrowser>
#include <ui/pxl/LauncherUpdateDialog.h>
#include <ui/pxl/WorkspacePage.h>
#include <ui/themes/ConeTheme.h>
#include <QStyleFactory>

class ReleaseReply final : public QNetworkReply {
public:
    ReleaseReply(const QNetworkRequest& request, QByteArray body, int status, QObject* parent)
        : QNetworkReply(parent), m_body(std::move(body))
    {
        setRequest(request); setUrl(request.url());
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute,status);
        open(QIODevice::ReadOnly);
        QTimer::singleShot(0,this,[this] {
            if (isFinished()) return;
            setFinished(true); emit readyRead(); emit finished();
        });
    }
    void abort() override
    {
        if (isFinished()) return;
        setError(OperationCanceledError,"aborted"); setFinished(true); emit finished();
    }
    qint64 bytesAvailable() const override { return m_body.size() - m_offset + QNetworkReply::bytesAvailable(); }
protected:
    qint64 readData(char* data,qint64 max) override
    {
        const auto length = qMin(max,qint64(m_body.size()-m_offset));
        if (!length) return -1;
        memcpy(data,m_body.constData()+m_offset,size_t(length)); m_offset+=length; return length;
    }
private:
    QByteArray m_body;
    qint64 m_offset = 0;
};
class ReleaseNetwork final : public QNetworkAccessManager {
public:
    QByteArray body = "[]";
    int status = 200, requests = 0;
    QUrl lastUrl;
protected:
    QNetworkReply* createRequest(Operation,const QNetworkRequest& request,QIODevice*) override
    {
        ++requests; lastUrl=request.url();
        return new ReleaseReply(request,body,status,this);
    }
};

class LauncherUpdatesTest : public QObject {
    Q_OBJECT
private:
    QByteArray releases() const
    {
        return R"([{"tag_name":"v12.1.0","name":"PXL Cone 12.1","body":"## Changes\n\n- **Better library**\n- Downloads fixed","prerelease":false,"draft":false},
        {"tag_name":"v13.0.0-beta.1","prerelease":true}, {"tag_name":"v99.0.0","draft":true},
        {"tag_name":"not-a-version"}, {"tag_name":"v12.0.0"}])";
    }
private slots:
    void initTestCase()
    {
        ConeTheme theme;
        qApp->setStyle(QStyleFactory::create(theme.qtTheme()));
        qApp->setPalette(theme.colorScheme());
        qApp->setStyleSheet(theme.appStyleSheet());
    }
    void selection()
    {
        QString error;
        auto selected=ReleaseUpdateChecker::selectRelease(releases(),"kakixc/PXL-Cone-Launcher","12.0.0",false,&error);
        QVERIFY(error.isEmpty()); QCOMPARE(selected.tag,QString("v12.1.0"));
        QCOMPARE(selected.url,QUrl("https://github.com/kakixc/PXL-Cone-Launcher/releases/tag/v12.1.0"));
        QCOMPARE(ReleaseUpdateChecker::selectRelease(releases(),"a/b","12.0.0",true,&error).tag,QString("v13.0.0-beta.1"));
        QVERIFY(!ReleaseUpdateChecker::selectRelease(releases(),"a/b","14.0.0",false,&error).isAvailable());
        QVERIFY(!ReleaseUpdateChecker::selectRelease(releases(),"a/b","12.1.0",false,&error).isAvailable());
        QVERIFY(!ReleaseUpdateChecker::selectRelease("{}","a/b","12.0.0",false,&error).isAvailable());
        QVERIFY(!error.isEmpty());
        QCOMPARE(ReleaseUpdateChecker::selectRelease(R"([{"tag_name":"v0.1.0-alpha.2","prerelease":true}])","a/b","0.1.0-alpha.1",true,&error).tag,QString("v0.1.0-alpha.2"));
        QCOMPARE(ReleaseUpdateChecker::selectRelease(R"([{"tag_name":"v0.1.0-alpha","prerelease":true}])","a/b","0.0.0-dev",true,&error).tag,QString("v0.1.0-alpha"));
    }
    void asynchronousStateAndErrors()
    {
        QTemporaryDir dir; ReleaseNetwork network; network.body=releases();
        ReleaseUpdateChecker checker(&network,dir.filePath("updates.ini"),"kakixc/PXL-Cone-Launcher","12.0.0");
        checker.setAutomaticallyChecksForUpdates(false);
        QSignalSpy changed(&checker,&ReleaseUpdateChecker::stateChanged);
        checker.checkForUpdates(); checker.checkForUpdates();
        QVERIFY(checker.checking()); QCOMPARE(network.requests,1);
        QTRY_VERIFY(!checker.checking()); QVERIFY(checker.error().isEmpty());
        QVERIFY(checker.release().isAvailable()); QVERIFY(changed.count()>=2);
        {
            ReleaseUpdateChecker reopened(&network,dir.filePath("updates.ini"),"kakixc/PXL-Cone-Launcher","12.0.0");
            QCOMPARE(reopened.release().tag,QString("v12.1.0"));
            ReleaseUpdateChecker upgraded(&network,dir.filePath("updates.ini"),"kakixc/PXL-Cone-Launcher","12.1.0");
            QVERIFY(!upgraded.release().isAvailable());
            ReleaseUpdateChecker otherRepo(&network,dir.filePath("updates.ini"),"another/repository","12.0.0");
            QVERIFY(!otherRepo.release().isAvailable());
        }
        QCOMPARE(network.lastUrl.host(),QString("api.github.com"));
        network.status=503; checker.checkForUpdates(); QTRY_VERIFY(!checker.checking());
        QVERIFY(!checker.error().isEmpty()); QCOMPARE(checker.release().tag,QString("v12.1.0"));
        network.status=200; network.body="broken"; checker.checkForUpdates(); QTRY_VERIFY(!checker.checking());
        QVERIFY(!checker.error().isEmpty()); QCOMPARE(checker.release().tag,QString("v12.1.0"));
        network.body="[]"; checker.checkForUpdates(); QTRY_VERIFY(!checker.checking());
        QVERIFY(checker.error().isEmpty()); QVERIFY(!checker.release().isAvailable());
        ReleaseUpdateChecker restored(&network,dir.filePath("updates.ini"),"a/b","12.0.0");
        QVERIFY(!restored.getAutomaticallyChecksForUpdates());
        network.body=R"([{"tag_name":"v13.0.0","prerelease":true}])";
        checker.setBetaAllowed(true); checker.checkForUpdates(); QTRY_VERIFY(!checker.checking());
        QVERIFY(checker.release().prerelease);
        checker.setBetaAllowed(false); QVERIFY(!checker.release().isAvailable());
        ReleaseUpdateChecker stable(&network,dir.filePath("updates.ini"),"kakixc/PXL-Cone-Launcher","12.0.0");
        QVERIFY(!stable.release().isAvailable());
    }
    void markdownAndHeader()
    {
        QTemporaryDir dir; ReleaseNetwork network; network.body=releases();
        ReleaseUpdateChecker checker(&network,dir.filePath("updates.ini"),"kakixc/PXL-Cone-Launcher","12.0.0");
        checker.setAutomaticallyChecksForUpdates(false);
        WorkspacePage workspace(nullptr); workspace.resize(1080,820); workspace.setImageBackground(true); workspace.bindUpdater(&checker); workspace.show();
        auto* notification=workspace.findChild<QPushButton*>("workspaceUpdateAvailable");
        QVERIFY(notification); QVERIFY(notification->isVisible()); QVERIFY(!notification->property("available").toBool());
        checker.checkForUpdates(); QTRY_VERIFY(!checker.checking()); QVERIFY(notification->isVisible()); QVERIFY(notification->property("available").toBool());
        QVERIFY(notification->width() >= QFontMetrics(notification->font()).horizontalAdvance(notification->text()) + 40);
        auto* profile = workspace.findChild<QPushButton*>("workspaceProfile");
        QVERIFY(profile);
        QTRY_VERIFY(!notification->geometry().intersects(profile->geometry()));
        Pxl::LauncherUpdateDialog dialog(&checker); dialog.show();
        auto* notes=dialog.findChild<QTextBrowser*>("releaseNotes");
        QVERIFY(notes); QVERIFY(notes->toPlainText().contains("Better library"));
        QVERIFY(!notes->toPlainText().contains("**"));
        const auto localImage = dir.filePath("private.png");
        QImage image(16,16,QImage::Format_ARGB32); image.fill(Qt::red);
        QVERIFY(image.save(localImage));
        QVERIFY(!notes->document()->resource(QTextDocument::ImageResource,QUrl::fromLocalFile(localImage)).isValid());
        QVERIFY(!notes->openExternalLinks()); QVERIFY(!notes->openLinks());
        if (const auto path=qEnvironmentVariable("PXL_UI_SCREENSHOT_DIR"); !path.isEmpty()) {
            workspace.grab().save(path+"/launcher-update-header.png");
            dialog.grab().save(path+"/launcher-update-notes.png");
        }
    }
    void oversizedResponseAndDestruction()
    {
        QTemporaryDir dir; ReleaseNetwork network;
        network.body=QByteArray(4*1024*1024+1,'x');
        auto* checker=new ReleaseUpdateChecker(&network,dir.filePath("updates.ini"),"a/b","12.0.0");
        checker->setAutomaticallyChecksForUpdates(false);
        checker->checkForUpdates(); QTRY_VERIFY(!checker->checking());
        QVERIFY(!checker->error().isEmpty()); QVERIFY(!checker->release().isAvailable());
        network.body="[]";
        checker->checkForUpdates(); QVERIFY(checker->checking());
        QPointer<QNetworkReply> pending=network.findChildren<QNetworkReply*>().last();
        delete checker;
        QTRY_VERIFY(pending.isNull());
    }
};
QTEST_MAIN(LauncherUpdatesTest)
#include "LauncherUpdates_test.moc"

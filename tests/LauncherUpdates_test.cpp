// SPDX-License-Identifier: GPL-3.0-only
#include <QTest>
#include <QLineEdit>
#include <algorithm>
#include <QHash>
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
#include <ui/pxl/AboutPage.h>
#include <ui/pxl/NumberField.h>
#include <ui/pagedialog/PageDialog.h>
#include <ui/widgets/PageContainer.h>
#include <QListView>
#include <QToolButton>
#include <QCheckBox>
#include <QMessageBox>
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

class SettingsPreviewPage final : public QWidget, public BasePage {
public:
    explicit SettingsPreviewPage(QString key) : m_key(std::move(key))
    {
        auto* layout = new QVBoxLayout(this);
        if (m_key == "launcher-settings") {
            auto* card = Pxl::settingsCard(this,"Library","Choose how your instances are organised.");
            auto* edit = new QLineEdit("Original value",card);
            edit->setObjectName("settingsFixtureValue");
            static_cast<QVBoxLayout*>(card->layout())->addWidget(edit);
            layout->addWidget(card);
            layout->addStretch();
        } else if (m_key == "appearance-settings") {
            layout->addWidget(new QLabel("Interface language",this));
            layout->addWidget(new QLabel("Choose the language used by PXL Cone.",this));
            layout->addStretch();
        }
    }
    QString id() const override { return m_key; }
    QString displayName() const override
    {
        const QHash<QString,QString> labels{{"launcher-settings","General"},{"appearance-settings","Appearance"},
            {"minecraft-settings","Minecraft"},{"java-settings","Java"},{"apis","Services"},
            {"external-tools","Tools"},{"proxy-settings","Proxy"}};
        return labels.value(m_key,m_key);
    }
    QIcon icon() const override { return {}; }
private:
    QString m_key;
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
        QVERIFY(notification); QVERIFY(!notification->isVisible()); QVERIFY(!notification->property("available").toBool());
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
    void aboutAndCompactSettingsNavigation()
    {
        WorkspacePage workspace(nullptr);
        workspace.resize(1080,820); workspace.setImageBackground(true);
        GenericPageProvider provider("Settings");
        for (const auto& key : {"launcher-settings","appearance-settings","minecraft-settings","java-settings",
                               "apis","external-tools","proxy-settings"})
            provider.addPageCreator([key] { return new SettingsPreviewPage(key); });
        int checks = 0;
        provider.addPageCreator([&] { return new Pxl::AboutPage(nullptr,[&] { ++checks; }); });
        PageDialog dialog(&provider,"about",nullptr,true);
        dialog.setProperty("pxlNavigation","workspaceSettings");
        QVERIFY(workspace.presentInstanceView(&dialog,"Settings",[] { return true; }));
        workspace.show();
        auto* navigation = dialog.findChild<QListView*>("conePageNavigation");
        auto* advanced = dialog.findChild<QCheckBox*>("pxlAdvancedSettingsMode");
        auto* search = dialog.findChild<QLineEdit*>("pxlSettingsSearch");
        auto* toolbar = dialog.findChild<QWidget*>("pxlSettingsToolbar");
        QVERIFY(navigation && advanced && search && toolbar);
        QCOMPARE(navigation->model()->rowCount(),5);
        QTest::qWait(30);
        QVERIFY(search->width()>0 && search->width()<=500);
        QVERIFY(qAbs(search->geometry().center().x()-toolbar->rect().center().x())<=2);
        QVERIFY(!search->geometry().intersects(advanced->geometry()));
        QVERIFY(navigation->geometry().top()<=toolbar->geometry().top()+1);
        QVERIFY(navigation->height()<dialog.height()*0.65);
        auto* apply=dialog.findChild<QPushButton*>("pxlApplySettings");
        QVERIFY(apply && !apply->isEnabled() && !apply->property("pxlPrimary").toBool());
        auto* heading=dialog.findChild<QLabel*>("conePageHeading");
        QVERIFY(heading && !heading->isVisible());
        const auto basicHeight = navigation->height();
        advanced->setChecked(true);
        QCOMPARE(navigation->model()->rowCount(),8);
        QTRY_VERIFY(navigation->height()>basicHeight);
        advanced->setChecked(false);
        QTRY_COMPARE(navigation->height(),basicHeight);
        workspace.resize(1600,900);
        QTRY_COMPARE(search->width(),500);
        QVERIFY(qAbs(search->geometry().center().x()-toolbar->rect().center().x())<=2);
        workspace.resize(1080,820);
        QTRY_VERIFY(search->width()<=500);
        for (const auto& query : {QString("language"),QString("launguage"),QString("interface langauge")}) {
            search->setText(query);
            QCOMPARE(navigation->model()->rowCount(),1);
            QCOMPARE(dialog.findChild<PageContainer*>()->selectedPage()->id(),QString("appearance-settings"));
        }
        search->setText("zzzxxyy nonexistent");
        QCOMPARE(navigation->model()->rowCount(),0);
        QVERIFY(!dialog.findChild<PageContainer*>()->selectedPage());
        QVERIFY(dialog.findChild<QWidget*>("pxlSettingsSearchEmpty")->isVisible());
        search->setText("About");
        QCOMPARE(navigation->model()->rowCount(),1);
        auto clearButtons=search->findChildren<QToolButton*>();
        QVERIFY(!clearButtons.isEmpty());
        auto* clear=*std::max_element(clearButtons.begin(),clearButtons.end(),[](auto* a,auto* b) { return a->geometry().right()<b->geometry().right(); });
        clear->click();
        QCOMPARE(search->text(),QString());
        QCOMPARE(navigation->model()->rowCount(),5);
        auto* check=dialog.findChild<QPushButton*>("aboutCheckUpdates");
        QVERIFY(check && check->isEnabled()); check->click(); QCOMPARE(checks,1);
        QTest::qWait(30);
        if (const auto path=qEnvironmentVariable("PXL_UI_SCREENSHOT_DIR"); !path.isEmpty())
            QVERIFY(workspace.grab().save(path+"/about-settings.png"));
        dialog.selectPage("launcher-settings");
        QTRY_VERIFY(heading->isVisible());
        QCOMPARE(toolbar->mapTo(&dialog,QPoint()).x(), heading->mapTo(&dialog,QPoint()).x());
        auto* value=dialog.findChild<QLineEdit*>("settingsFixtureValue");
        QVERIFY(value); value->setText("Changed value");
        QVERIFY(apply->isEnabled() && apply->property("pxlPrimary").toBool());
        apply->click();
        QVERIFY(!apply->isEnabled() && !apply->property("pxlPrimary").toBool());
        QTest::qWait(180);
        if (const auto path=qEnvironmentVariable("PXL_UI_SCREENSHOT_DIR"); !path.isEmpty())
            QVERIFY(workspace.grab().save(path+"/general-settings-grid.png"));
        dialog.selectPage("about");
        QVERIFY(!heading->isVisible());
        // Searching/clearing and disclosure controls must not prompt to save settings.
        bool prompted=false;
        QTimer watchdog;
        watchdog.setSingleShot(true);
        connect(&watchdog,&QTimer::timeout,&dialog,[&] {
            if (auto* prompt=dialog.findChild<QMessageBox*>()) { prompted=true; prompt->reject(); }
        });
        watchdog.start(200);
        QVERIFY(dialog.requestLeave());
        QVERIFY(!prompted);
    }
    void numberFieldPreservesOriginalSpinBehavior()
    {
        auto* original = new QSpinBox;
        original->setRange(128,2048); original->setSingleStep(128); original->setValue(512); original->setSuffix(" MiB");
        Pxl::NumberField field(original); field.show();
        QCOMPARE(original->buttonSymbols(),QAbstractSpinBox::NoButtons);
        field.findChild<QToolButton*>("pxlNumberIncrease")->click(); QCOMPARE(original->value(),640);
        field.findChild<QToolButton*>("pxlNumberDecrease")->click(); QCOMPARE(original->value(),512);
        original->setFocus(); QTest::keyClick(original,Qt::Key_Up); QCOMPARE(original->value(),640);
        QTest::keyClick(original,Qt::Key_A,Qt::ControlModifier); QTest::keyClicks(original,"1024"); QTest::keyClick(original,Qt::Key_Return);
        QCOMPARE(original->value(),1024); QCOMPARE(original->suffix(),QString(" MiB"));
        original->setValue(2048); field.findChild<QToolButton*>("pxlNumberIncrease")->click(); QCOMPARE(original->value(),2048);
        if (const auto path=qEnvironmentVariable("PXL_UI_SCREENSHOT_DIR"); !path.isEmpty())
            QVERIFY(field.grab().save(path+"/number-field.png"));
    }
    void hiddenWorkspaceUsesVisibleModalParent()
    {
        QTemporaryDir dir; ReleaseNetwork network;
        ReleaseUpdateChecker checker(&network,dir.filePath("updates.ini"),"a/b","12.0.0");
        checker.setAutomaticallyChecksForUpdates(false);
        WorkspacePage hidden(nullptr); hidden.bindUpdater(&checker);
        QDialog parent;
        parent.resize(900,700);
        bool visibleChild = false;
        QTimer watchdog;
        watchdog.setSingleShot(true);
        connect(&watchdog,&QTimer::timeout,&parent,[&] {
            for (auto* dialog : hidden.findChildren<QDialog*>()) dialog->reject();
            for (auto* dialog : parent.findChildren<QDialog*>()) dialog->reject();
            parent.reject();
        });
        QTimer::singleShot(0,&parent,[&] {
            QTimer::singleShot(30,&parent,[&] {
                auto* update = parent.findChild<QDialog*>("pxlLauncherUpdates");
                visibleChild = update && update->isVisible();
                if (update) update->reject();
            });
            hidden.showLauncherUpdates();
            parent.reject();
        });
        watchdog.start(1000);
        parent.exec();
        QVERIFY(visibleChild);
    }
};
QTEST_MAIN(LauncherUpdatesTest)
#include "LauncherUpdates_test.moc"

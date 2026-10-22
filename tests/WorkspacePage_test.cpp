// SPDX-License-Identifier: GPL-3.0-only

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QLineEdit>
#include <QPushButton>
#include <QDir>
#include <QApplication>
#include <QFontDatabase>
#include <QRawFont>
#include <QStyleFactory>

#include <memory>

#include <InstanceList.h>
#include <settings/INISettingsObject.h>
#include <ui/pxl/WorkspacePage.h>
#include <ui/themes/MinecraftTheme.h>

class WorkspacePageTest : public QObject {
    Q_OBJECT

   private:
    static std::unique_ptr<INISettingsObject> settings(const QTemporaryDir& temporary)
    {
        return std::make_unique<INISettingsObject>(temporary.filePath("global.cfg"));
    }

   private slots:
    void initTestCase()
    {
        if (qEnvironmentVariable("PXL_UI_THEME") == "minecraft") {
            MinecraftTheme theme;
            qApp->setStyle(QStyleFactory::create(theme.qtTheme()));
            qApp->setPalette(theme.colorScheme());
            qApp->setStyleSheet(theme.appStyleSheet());
            const int fontId = QFontDatabase::addApplicationFont(":/fonts/Monocraft.ttf");
            QVERIFY(fontId >= 0);
            const auto families = QFontDatabase::applicationFontFamilies(fontId);
            QVERIFY(!families.isEmpty());
            const auto font = QRawFont::fromFont(QFont(families.first(), 12));
            QVERIFY(font.supportsCharacter(QChar(u'И')));
            QVERIFY(font.supportsCharacter(QChar(u'ё')));
        }
    }

    void emptyLibraryIsNavigableAndSafe()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        auto globalSettings = settings(temporary);
        InstanceList instances(globalSettings.get(), temporary.filePath("instances"));
        WorkspacePage page(&instances);
        auto* library = page.findChild<QPushButton*>("workspaceLibrary");
        auto* search = page.findChild<QLineEdit*>("workspaceSearch");
        auto* play = page.findChild<QPushButton*>("workspacePlaySelected");
        QVERIFY(library);
        QVERIFY(search);
        QVERIFY(play);
        QVERIFY(page.findChild<QPushButton*>("workspaceContinue")->isHidden());
        QVERIFY(!play->isEnabled());

        const QString screenshotDirectory = qEnvironmentVariable("PXL_UI_SCREENSHOT_DIR");
        if (!screenshotDirectory.isEmpty()) {
            QVERIFY(QDir().mkpath(screenshotDirectory));
            page.resize(1080, 680);
            page.show();
            QCoreApplication::processEvents();
            QVERIFY(page.grab().save(QDir(screenshotDirectory).filePath("home.png")));
        }

        library->click();
        if (!screenshotDirectory.isEmpty()) {
            QCoreApplication::processEvents();
            QVERIFY(page.grab().save(QDir(screenshotDirectory).filePath("library.png")));
        }
        search->setText("missing");
        QVERIFY(search->text() == "missing");
        page.setSelectedInstance("does-not-exist");
        QSignalSpy activated(&page, &WorkspacePage::activateInstance);
        play->click();
        QCOMPARE(activated.count(), 0);
    }

    void navigationAndActionButtonsEmitRequests()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        auto globalSettings = settings(temporary);
        InstanceList instances(globalSettings.get(), temporary.filePath("instances"));
        WorkspacePage page(&instances);

        QSignalSpy create(&page, &WorkspacePage::createRequested);
        QSignalSpy import(&page, &WorkspacePage::importRequested);
        QSignalSpy exportRequest(&page, &WorkspacePage::exportRequested);
        QSignalSpy accounts(&page, &WorkspacePage::accountsRequested);
        QSignalSpy settingsRequest(&page, &WorkspacePage::settingsRequested);
        QSignalSpy classic(&page, &WorkspacePage::classicRequested);

        page.findChild<QPushButton*>("workspaceHomeCreate")->click();
        page.findChild<QPushButton*>("workspaceHomeImport")->click();
        page.findChild<QPushButton*>("workspaceHomeExport")->click();
        page.findChild<QPushButton*>("workspaceAccounts")->click();
        page.findChild<QPushButton*>("workspaceSettings")->click();
        page.findChild<QPushButton*>("workspaceClassic")->click();

        QCOMPARE(create.count(), 1);
        QCOMPARE(import.count(), 1);
        QCOMPARE(exportRequest.count(), 1);
        QCOMPARE(accounts.count(), 1);
        QCOMPARE(settingsRequest.count(), 1);
        QCOMPARE(classic.count(), 1);
    }
};

QTEST_MAIN(WorkspacePageTest)
#include "WorkspacePage_test.moc"

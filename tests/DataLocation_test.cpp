// SPDX-License-Identifier: GPL-3.0-only
#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QStandardPaths>
#include "BuildConfig.h"
#include "pxl/DataLocation.h"

class DataLocationTest : public QObject {
    Q_OBJECT
private slots:
    void overridePrecedence()
    {
        QProcessEnvironment environment;
        QCOMPARE(Pxl::dataDirectoryOverride(environment, "ElyPrismLauncher"), QString());
        environment.insert("ELYPRISMLAUNCHER_DATA_DIR", "legacy selected path");
        QCOMPARE(Pxl::dataDirectoryOverride(environment, "ElyPrismLauncher"), QString("legacy selected path"));
        environment.insert("PXLCONE_DATA_DIR", "PXL selected path");
        QCOMPARE(Pxl::dataDirectoryOverride(environment, "ElyPrismLauncher"), QString("PXL selected path"));
    }
    void locationPrecedence()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const auto root = temp.path() + "/portable";
        const auto userData = root + "/UserData";
        const auto persistent = temp.path() + "/PXLConeLauncher";
        QVERIFY(QDir().mkpath(userData));
        QFile marker(root + "/portable.txt");
        QVERIFY(marker.open(QIODevice::WriteOnly));
        marker.close();

        auto selection = Pxl::selectDataLocation("explicit", "environment", root, persistent, true);
        QCOMPARE(selection.path, QString("explicit"));
        QVERIFY(!selection.portable);
        selection = Pxl::selectDataLocation({}, "environment", root, persistent, true);
        QCOMPARE(selection.path, QString("environment"));
        QVERIFY(!selection.portable);
        selection = Pxl::selectDataLocation({}, {}, root, persistent, true);
        QCOMPARE(selection.path, userData);
        QVERIFY(selection.portable);
        QVERIFY(QDir().rmdir(userData));
        selection = Pxl::selectDataLocation({}, {}, root, persistent, true);
        QCOMPARE(selection.path, root);
        QVERIFY(selection.portable);
        QVERIFY(marker.remove());
        selection = Pxl::selectDataLocation({}, {}, root, persistent, true);
        QCOMPARE(selection.path, persistent);
        QVERIFY(!selection.portable);
        QVERIFY(!QFileInfo::exists(persistent)); // Selection has no write side effects.
    }
    void disabledPortableAndLegacyIsolation()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const auto root = temp.path() + "/portable";
        QVERIFY(QDir().mkpath(root + "/UserData"));
        const auto legacy = temp.path() + "/ElyPrismLauncher";
        QVERIFY(QDir().mkpath(legacy));
        QFile sentinel(legacy + "/keep.txt");
        QVERIFY(sentinel.open(QIODevice::WriteOnly));
        QCOMPARE(sentinel.write("keep"), qint64(4));
        sentinel.close();
        const auto persistent = temp.path() + "/PXLConeLauncher";
        const auto selection = Pxl::selectDataLocation({}, {}, root, persistent, false);
        QCOMPARE(selection.path, persistent);
        QVERIFY(!selection.portable);
        QVERIFY(!QFileInfo::exists(persistent));
        QVERIFY(sentinel.open(QIODevice::ReadOnly));
        QCOMPARE(sentinel.readAll(), QByteArray("keep"));
        QCOMPARE(Pxl::selectDataLocation(legacy, {}, root, persistent, false).path, legacy);
    }
    void independentQtIdentityAndExistingConfigName()
    {
        QCOMPARE(BuildConfig.LAUNCHER_RUNTIME_NAME, QString("PXLConeLauncher"));
        QVERIFY(BuildConfig.LAUNCHER_RUNTIME_NAME != BuildConfig.LAUNCHER_NAME);
        QCOMPARE(BuildConfig.LAUNCHER_CONFIGFILE, BuildConfig.LAUNCHER_APP_BINARY_NAME + ".cfg");
        const auto oldOrganization = QCoreApplication::organizationName();
        const auto oldApplication = QCoreApplication::applicationName();
        QCoreApplication::setOrganizationName(BuildConfig.LAUNCHER_NAME);
        QCoreApplication::setApplicationName(BuildConfig.LAUNCHER_NAME);
        const auto legacy = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/..").absolutePath();
        QCoreApplication::setOrganizationName(BuildConfig.LAUNCHER_RUNTIME_NAME);
        QCoreApplication::setApplicationName(BuildConfig.LAUNCHER_RUNTIME_NAME);
        const auto current = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/..").absolutePath();
        QCoreApplication::setOrganizationName(oldOrganization);
        QCoreApplication::setApplicationName(oldApplication);
        QVERIFY(current != legacy);
        QCOMPARE(QFileInfo(current).fileName(), BuildConfig.LAUNCHER_RUNTIME_NAME);
    }
};

QTEST_GUILESS_MAIN(DataLocationTest)
#include "DataLocation_test.moc"

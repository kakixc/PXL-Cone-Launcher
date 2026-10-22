// SPDX-License-Identifier: GPL-3.0-only
#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QDir>
#include "pxl/WorkspaceTransfer.h"
#include "archive/ArchiveWriter.h"
#include "archive/ArchiveReader.h"
#include "settings/INIFile.h"
#include "modplatform/packwiz/Packwiz.h"

class WorkspaceTransferTest : public QObject {
    Q_OBJECT
    static void put(const QString& path, const QByteArray& bytes)
    {
        QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(bytes), bytes.size());
    }
    static Pxl::Workspace fixture(const QString& root)
    {
        const QDir dir(root);
        put(dir.filePath("instance.cfg"), "[General]\nInstanceType=OneSix\nname=Survival\nPreLaunchCommand=bad\nJavaPath=/old/machine/java\nMaxMemAlloc=4096\nOverrideMemory=true\n");
        put(dir.filePath("mmc-pack.json"), R"({"formatVersion":1,"components":[{"uid":"net.minecraft","version":"1.20.1"},{"uid":"net.fabricmc.fabric-loader","version":"0.16.14"}]})");
        put(dir.filePath(".minecraft/mods/local.jar"), "local mod bytes");
        put(dir.filePath(".minecraft/config/settings.json"), "{\"value\":3}");
        put(dir.filePath(".minecraft/saves/World/level.dat"), "irreplaceable world");
        put(dir.filePath(".minecraft/screenshots/test.png"), "screenshot");
        put(dir.filePath(".minecraft/accounts.json"), "secret");
        put(dir.filePath(".minecraft/logs/latest.log"), "private log");
        Pxl::Workspace workspace;
        workspace.root = root;
        workspace.name = "Survival";
        workspace.group = "Favorites";
        return workspace;
    }

   private slots:
    void unsafePaths_data()
    {
        QTest::addColumn<QString>("path");
        for (const auto& path : { "../outside", "/absolute", "C:/file", "minecraft/../file", "minecraft//file", "a\\b", "AUX.txt", "file:ads", "trailing.", "a/NUL", "a/b " })
            QTest::newRow(path) << QString(path);
    }
    void unsafePaths()
    {
        QFETCH(QString, path);
        QVERIFY(!Pxl::safeRelativePath(path));
        QVERIFY(Pxl::safeRelativePath("minecraft/saves/My World/level.dat"));
    }
    void roundTripOffline()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const auto workspace = fixture(temp.path() + "/source/instances/test");
        const auto archive = temp.path() + "/pack.pxlconep";
        QVERIFY2(Pxl::exportArchive(archive, { workspace }, { "pxlconep", "offline", true, false }).isEmpty(), "export failed");
        Pxl::Manifest manifest;
        QCOMPARE(Pxl::inspectArchive(archive, manifest), QString());
        QCOMPARE(manifest.instances.size(), 1);
        QCOMPARE(manifest.instances[0].group, QString("Favorites"));
        const auto destination = temp.path() + "/restored";
        QVERIFY(QDir().mkpath(destination));
        Pxl::Workspace restored;
        QCOMPARE(Pxl::prepareArchiveInstance(archive, 0, destination, restored), QString());
        QCOMPARE(Pxl::verifyInstance(restored, destination), QString());
        QCOMPARE(Pxl::fileHash(destination + "/minecraft/saves/World/level.dat"),
                 Pxl::fileHash(workspace.root + "/.minecraft/saves/World/level.dat"));
        QVERIFY(!QFileInfo::exists(destination + "/minecraft/accounts.json"));
        QVERIFY(!QFileInfo::exists(destination + "/minecraft/logs/latest.log"));
        QVERIFY(!QFileInfo::exists(destination + "/minecraft/screenshots/test.png"));
        INIFile cfg;
        QVERIFY(cfg.loadFile(destination + "/instance.cfg"));
        QVERIFY(!cfg.contains("PreLaunchCommand"));
        QVERIFY(!cfg.contains("JavaPath"));
        QCOMPARE(cfg.get("MaxMemAlloc", 0).toInt(), 4096);
        put(destination + "/minecraft/mods/local.jar", "corrupt");
        QVERIFY(!Pxl::verifyInstance(restored, destination).isEmpty());
    }
    void transferTwoInstancesAndDefaultWorldExclusion()
    {
        QTemporaryDir temp;
        auto first = fixture(temp.path() + "/one");
        auto second = fixture(temp.path() + "/two");
        second.name = "Other";
        const auto path = temp.path() + "/transfer.pxlt";
        QCOMPARE(Pxl::exportArchive(path, { first, second }, { "pxlt", "thin", false, true }), QString());
        Pxl::Manifest manifest;
        QCOMPARE(Pxl::inspectArchive(path, manifest), QString());
        QCOMPARE(manifest.instances.size(), 2);
        for (int i = 0; i < 2; ++i) {
            const auto destination = temp.path() + QString("/restored%1").arg(i);
            QVERIFY(QDir().mkpath(destination));
            Pxl::Workspace restored;
            QCOMPARE(Pxl::prepareArchiveInstance(path, i, destination, restored), QString());
            QCOMPARE(Pxl::verifyInstance(restored, destination), QString());
            QVERIFY(!QFileInfo::exists(destination + "/minecraft/saves"));
            QVERIFY(QFileInfo::exists(destination + "/minecraft/screenshots/test.png"));
        }
    }
    void rejectMalformedManifestsAndReferences()
    {
        QTemporaryDir temp;
        auto workspace = fixture(temp.path() + "/source");
        const auto path = temp.path() + "/pack.pxlconep";
        QCOMPARE(Pxl::exportArchive(path, { workspace }, {}), QString());
        Pxl::Manifest manifest;
        QCOMPARE(Pxl::inspectArchive(path, manifest), QString());
        auto object = Pxl::toJson(manifest);
        object["formatVersion"] = 99;
        Pxl::Manifest parsed;
        QVERIFY(!Pxl::fromJson(object, parsed).isEmpty());
        object = Pxl::toJson(manifest);
        object["requires"] = QJsonArray{ "servers" };
        QVERIFY(!Pxl::fromJson(object, parsed).isEmpty());
        auto& files = manifest.instances[0].files;
        files.append(files.first());
        QVERIFY(!Pxl::fromJson(Pxl::toJson(manifest), parsed).isEmpty());
        files.removeLast();
        files[0].bundled = false;
        files[0].url = "http://127.0.0.1/secret";
        QVERIFY(!Pxl::fromJson(Pxl::toJson(manifest), parsed).isEmpty());
        files[0].url = "https://cdn.modrinth.com/file.jar";
        files[0].source = "modrinth";
        files[0].projectId = "exact-project";
        files[0].versionId = "exact-version";
        QCOMPARE(Pxl::fromJson(Pxl::toJson(manifest), parsed), QString());
    }
    void rejectUnsafeArchiveAndCorruption()
    {
        QTemporaryDir temp;
        const auto archive = temp.path() + "/bad.pxlconep";
        {
            MMCZip::ArchiveWriter writer(archive);
            QVERIFY(writer.open());
            QVERIFY(writer.addFile("../outside", QByteArray("bad")));
            QVERIFY(writer.close());
        }
        Pxl::Manifest manifest;
        QVERIFY(!Pxl::inspectArchive(archive, manifest).isEmpty());
        QVERIFY(!QFileInfo::exists(temp.path() + "/outside"));
        auto source = fixture(temp.path() + "/source");
        QCOMPARE(Pxl::exportArchive(archive, { source }, { "pxlconep", "offline", false, false }), QString());
        QCOMPARE(Pxl::inspectArchive(archive, manifest), QString());
        {
            MMCZip::ArchiveWriter writer(archive);
            QVERIFY(writer.open());
            QVERIFY(writer.addFile("manifest.json", QJsonDocument(Pxl::toJson(manifest)).toJson()));
            for (const auto& file : manifest.instances[0].files)
                QVERIFY(writer.addFile("instances/0/" + file.path, QByteArray(file.size, 'x')));
            QVERIFY(writer.close());
        }
        const auto destination = temp.path() + "/restored";
        QVERIFY(QDir().mkpath(destination));
        Pxl::Workspace restored;
        QVERIFY(!Pxl::prepareArchiveInstance(archive, 0, destination, restored).isEmpty());
        QVERIFY(!QFileInfo::exists(destination + "/instance.cfg"));
    }
    void sourceCopyAndCancellation()
    {
        QTemporaryDir temp;
        const auto workspace = fixture(temp.path() + "/launcher/instances/source");
        QVector<Pxl::Workspace> found;
        QCOMPARE(Pxl::scanLauncher(temp.path() + "/launcher", found), QString());
        QCOMPARE(found.size(), 1);
        const auto original = Pxl::fileHash(workspace.root + "/.minecraft/saves/World/level.dat");
        const auto destination = temp.path() + "/imported";
        QVERIFY(QDir().mkpath(destination));
        QCOMPARE(Pxl::copyLauncherInstance(found[0], destination), QString());
        QCOMPARE(Pxl::fileHash(destination + "/minecraft/saves/World/level.dat"), original);
        QCOMPARE(Pxl::fileHash(workspace.root + "/.minecraft/saves/World/level.dat"), original);
        QVERIFY(!Pxl::copyLauncherInstance(found[0], workspace.root).isEmpty());
        std::atomic_bool cancel{ true };
        const auto path = temp.path() + "/cancelled.pxlconep";
        put(path, "existing archive");
        const auto hash = Pxl::fileHash(path);
        QVERIFY(!Pxl::exportArchive(path, { workspace }, {}, &cancel).isEmpty());
        QCOMPARE(Pxl::fileHash(path), hash);
    }
    void smartPackUsesExistingMetadataAndBundlesModifiedFiles()
    {
        QTemporaryDir temp;
        auto workspace = fixture(temp.path() + "/source");
        Packwiz::V1::Mod metadata;
        metadata.slug = "project-slug-not-file-name";
        metadata.name = "Known Mod";
        metadata.filename = "local.jar";
        metadata.provider = ModPlatform::ResourceProvider::MODRINTH;
        metadata.project_id = "project-id";
        metadata.file_id = "exact-version";
        metadata.url = QUrl("https://cdn.modrinth.com/data/project-id/versions/exact-version/local.jar");
        metadata.mode = "url";
        metadata.hash_format = "sha256";
        metadata.hash = Pxl::fileHash(workspace.root + "/.minecraft/mods/local.jar");
        const QDir index(workspace.root + "/.minecraft/mods/.index");
        QVERIFY(QDir().mkpath(index.path()));
        Packwiz::V1::updateModIndex(index, metadata);
        const auto path = temp.path() + "/smart.pxlconep";
        QCOMPARE(Pxl::exportArchive(path, { workspace }, {}), QString());
        Pxl::Manifest manifest;
        QCOMPARE(Pxl::inspectArchive(path, manifest), QString());
        bool found = false;
        for (const auto& file : manifest.instances.first().files) {
            if (file.path != "minecraft/mods/local.jar") continue;
            found = true;
            QVERIFY(!file.bundled);
            QCOMPARE(file.projectId, QString("project-id"));
            QCOMPARE(file.versionId, QString("exact-version"));
        }
        QVERIFY(found);
        put(workspace.root + "/.minecraft/mods/local.jar", "locally changed mod");
        QCOMPARE(Pxl::exportArchive(path, { workspace }, {}), QString());
        QCOMPARE(Pxl::inspectArchive(path, manifest), QString());
        for (const auto& file : manifest.instances.first().files)
            if (file.path == "minecraft/mods/local.jar") QVERIFY(file.bundled);
    }
    void workspaceMetadataSurvivesRoundTrip()
    {
        QTemporaryDir temp;
        auto workspace = fixture(temp.path() + "/source");
        const QByteArray metadata = R"({"serverRelationship":{"id":"local-server","type":"fabric"},"custom":{"note":"preserve"}})";
        put(workspace.root + "/pxl-workspace.json", metadata);
        const auto path = temp.path() + "/metadata.pxlconep";
        QCOMPARE(Pxl::exportArchive(path, { workspace }, {}), QString());
        const auto staging = temp.path() + "/staging";
        QVERIFY(QDir().mkpath(staging));
        Pxl::Workspace restored;
        QCOMPARE(Pxl::prepareArchiveInstance(path, 0, staging, restored), QString());
        QCOMPARE(restored.metadata, QJsonDocument::fromJson(metadata).object());
        QFile saved(staging + "/pxl-workspace.json");
        QVERIFY(saved.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(saved.readAll()).object(), restored.metadata);
    }
    void launcherPreviewReportsVersionAndWorldPresence()
    {
        QTemporaryDir temp;
        fixture(temp.path() + "/launcher/instances/with-world");
        put(temp.path() + "/launcher/instances/empty/instance.cfg", "InstanceType=OneSix\nname=Empty\n");
        put(temp.path() + "/launcher/instances/empty/mmc-pack.json",
            R"({"formatVersion":1,"components":[{"uid":"net.minecraft","version":"1.21.1"}]})");
        QVector<Pxl::Workspace> found;
        QCOMPARE(Pxl::scanLauncher(temp.path() + "/launcher", found), QString());
        QCOMPARE(found.size(), 2);
        for (const auto& instance : found) {
            if (instance.name == "Empty") {
                QCOMPARE(instance.minecraftVersion, QString("1.21.1"));
                QVERIFY(!instance.worldsPresent);
                QVERIFY(!instance.screenshotsPresent);
            } else {
                QCOMPARE(instance.minecraftVersion, QString("1.20.1"));
                QVERIFY(instance.worldsPresent);
                QVERIFY(instance.screenshotsPresent);
            }
        }
    }
    void cancellationDuringExtractionDoesNotCreateInstanceMetadata()
    {
        QTemporaryDir temp;
        auto workspace = fixture(temp.path() + "/source");
        const auto archive = temp.path() + "/pack.pxlconep";
        QCOMPARE(Pxl::exportArchive(archive, { workspace }, { "pxlconep", "offline", true, true }), QString());
        const auto staging = temp.path() + "/staging";
        QVERIFY(QDir().mkpath(staging));
        std::atomic_bool cancelled{ false };
        Pxl::Workspace restored;
        const auto error = Pxl::prepareArchiveInstance(archive, 0, staging, restored, &cancelled,
            [&cancelled](qint64, qint64, const QString&) { cancelled.store(true); });
        QVERIFY(!error.isEmpty());
        QVERIFY(cancelled.load());
        QVERIFY(!QFileInfo::exists(staging + "/instance.cfg"));
        QCOMPARE(Pxl::fileHash(workspace.root + "/.minecraft/saves/World/level.dat"),
                 QString::fromLatin1(QCryptographicHash::hash("irreplaceable world", QCryptographicHash::Sha256).toHex()));
    }
    void rejectCaseAndDirectoryCollisions()
    {
        QTemporaryDir temp;
        const auto workspace = fixture(temp.path() + "/source");
        const auto archive = temp.path() + "/pack.pxlconep";
        QCOMPARE(Pxl::exportArchive(archive, { workspace }, {}), QString());
        Pxl::Manifest manifest, parsed;
        QCOMPARE(Pxl::inspectArchive(archive, manifest), QString());
        const auto original = manifest;
        auto duplicate = manifest.instances[0].files.first();
        duplicate.path = duplicate.path.toUpper();
        manifest.instances[0].files.append(duplicate);
        QVERIFY(!Pxl::fromJson(Pxl::toJson(manifest), parsed).isEmpty());
        manifest = original;
        duplicate = manifest.instances[0].files.first();
        duplicate.path = duplicate.path.left(duplicate.path.lastIndexOf('/'));
        manifest.instances[0].files.append(duplicate);
        QVERIFY(!Pxl::fromJson(Pxl::toJson(manifest), parsed).isEmpty());
    }
    void malformedLaterHeaderIsNotEndOfArchive()
    {
        QTemporaryDir temp;
        QByteArray header(512, '\0');
        header.replace(0, 3, "one");
        header.replace(100, 8, QByteArray("0000644\0", 8));
        header.replace(108, 8, QByteArray("0000000\0", 8));
        header.replace(116, 8, QByteArray("0000000\0", 8));
        header.replace(124, 12, QByteArray("00000000001\0", 12));
        header.replace(136, 12, QByteArray("00000000000\0", 12));
        header.replace(148, 8, QByteArray(8, ' '));
        header[156] = '0';
        header.replace(257, 6, QByteArray("ustar\0", 6));
        header.replace(263, 2, "00");
        unsigned checksum = 0;
        for (const auto byte : header) checksum += static_cast<unsigned char>(byte);
        const auto octal = QByteArray::number(checksum, 8).rightJustified(6, '0') + QByteArray("\0 ", 2);
        header.replace(148, 8, octal);
        const auto path = temp.path() + "/broken.tar";
        put(path, header + QByteArray("a") + QByteArray(511, '\0') + QByteArray(512, '!') + QByteArray(1024, '\0'));
        MMCZip::ArchiveReader reader(path);
        int seen = 0;
        QVERIFY(!reader.parse([&seen](MMCZip::ArchiveReader::File*) { ++seen; return true; }));
        QCOMPARE(seen, 1);
    }
};

QTEST_GUILESS_MAIN(WorkspaceTransferTest)
#include "WorkspaceTransfer_test.moc"

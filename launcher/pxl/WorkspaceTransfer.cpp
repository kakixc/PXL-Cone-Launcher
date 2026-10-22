// SPDX-License-Identifier: GPL-3.0-only
#include "WorkspaceTransfer.h"

#include <QCryptographicHash>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QUrl>

#include "archive/ArchiveReader.h"
#include "archive/ArchiveWriter.h"
#include "modplatform/packwiz/Packwiz.h"
#include "settings/INIFile.h"

namespace Pxl {
namespace {
constexpr qint64 ManifestLimit = 16 * 1024 * 1024;
constexpr qint64 FileLimit = 32LL * 1024 * 1024 * 1024;
constexpr qint64 TotalLimit = 256LL * 1024 * 1024 * 1024;
constexpr int EntryLimit = 500000;
const QStringList PortableSettings = { "notes", "OverrideMemory", "MinMemAlloc", "MaxMemAlloc", "PermGen",
                                       "OverrideWindow", "MinecraftWinWidth", "MinecraftWinHeight", "LaunchMaximized",
                                       "lastLaunchTime", "lastTimePlayed", "totalTimePlayed", "ManagedPack", "ManagedPackType",
                                       "ManagedPackID", "ManagedPackName", "ManagedPackVersionID", "ManagedPackVersionName", "ManagedPackURL" };

struct Failure { QString message; };
void require(bool condition, const QString& message)
{
    if (!condition)
        throw Failure{ message };
}
void checkCancelled(std::atomic_bool* cancel)
{
    require(!cancel || !cancel->load(), QObject::tr("Cancelled."));
}
QByteArray readSmall(const QString& path, qint64 limit = ManifestLimit)
{
    QFile file(path);
    require(file.open(QIODevice::ReadOnly) && file.size() <= limit, QObject::tr("Cannot read metadata: %1").arg(path));
    auto bytes = file.read(limit + 1);
    require(file.error() == QFileDevice::NoError && bytes.size() <= limit, QObject::tr("Metadata is unreadable or too large: %1").arg(path));
    return bytes;
}
QJsonObject readObject(const QByteArray& bytes)
{
    QJsonParseError error;
    auto document = QJsonDocument::fromJson(bytes, &error);
    require(error.error == QJsonParseError::NoError && document.isObject(), QObject::tr("Invalid JSON manifest: %1").arg(error.errorString()));
    return document.object();
}
bool contentPath(const QString& path)
{
    return safeRelativePath(path) && (path.startsWith("minecraft/") || path.startsWith("patches/") ||
                                      path.startsWith("jarmods/") || path == "icon.png");
}
bool trustedDownload(const QString& text)
{
    const QUrl url(text);
    const QString host = url.host().toLower();
    return url.isValid() && url.scheme() == "https" && url.userInfo().isEmpty() && url.port(-1) == -1 &&
           (host == "cdn.modrinth.com" || host == "edge.forgecdn.net" || host == "mediafilez.forgecdn.net" ||
            host == "media.forgecdn.net");
}
QString canonicalKey(const QString& path)
{
    return path.normalized(QString::NormalizationForm_C).toCaseFolded();
}
struct PathSet {
    QSet<QString> files;
    QSet<QString> directories;
    QSet<QString> explicitDirectories;
};
void addPath(PathSet& paths, const QString& path, bool directory = false)
{
    const auto key = canonicalKey(path);
    require(!paths.files.contains(key) && (directory ? !paths.explicitDirectories.contains(key) : !paths.directories.contains(key)),
            QObject::tr("Duplicate or case-colliding path: %1").arg(path));
    QString ancestor = key;
    while (ancestor.contains('/')) {
        ancestor = ancestor.left(ancestor.lastIndexOf('/'));
        require(!paths.files.contains(ancestor), QObject::tr("File/directory collision: %1").arg(path));
        paths.directories.insert(ancestor);
    }
    if (directory) {
        paths.directories.insert(key);
        paths.explicitDirectories.insert(key);
    } else paths.files.insert(key);
}
void writeBytes(const QString& path, const QByteArray& bytes)
{
    require(QDir().mkpath(QFileInfo(path).absolutePath()), QObject::tr("Cannot create directory for %1").arg(path));
    QSaveFile file(path);
    require(file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit(), QObject::tr("Cannot write %1").arg(path));
}
void writeMetadata(const Workspace& workspace, const QString& staging)
{
    INIFile settings;
    settings.set("InstanceType", "OneSix");
    settings.set("name", workspace.name);
    // Keep custom commands, Java paths and machine-specific environment out of imports.
    for (const auto& key : PortableSettings) {
        if (workspace.settings.contains(key))
            settings.set(key, workspace.settings[key].toVariant());
    }
    settings.set("iconKey", "default");
    require(settings.saveFile(QDir(staging).filePath("instance.cfg")), QObject::tr("Cannot write instance settings."));
    writeBytes(QDir(staging).filePath("mmc-pack.json"), QJsonDocument(workspace.components).toJson());
    if (!workspace.metadata.isEmpty())
        writeBytes(QDir(staging).filePath("pxl-workspace.json"), QJsonDocument(workspace.metadata).toJson());
}
bool includedGamePath(const QString& path, const ExportOptions& options)
{
    const QString first = path.section('/', 0, 0).toLower();
    static const QSet<QString> excluded = { "logs", "crash-reports", "natives", ".git", ".cache", "webcache", "cache",
                                            "accounts.json", "launcher_accounts.json", "launcher_profiles.json",
                                            "usercache.json", "usernamecache.json", "session.json", "auth.json" };
    if (excluded.contains(first) || path.endsWith(".lock") || path == "session.lock")
        return false;
    return (options.worlds || first != "saves") && (options.screenshots || first != "screenshots");
}
void collectTree(const QString& root, const QString& prefix, const ExportOptions& options, QVector<ContentFile>& files,
                 QHash<QString, QString>& sources, std::atomic_bool* cancel)
{
    if (!QFileInfo::exists(root))
        return;
    require(!QFileInfo(root).isSymLink(), QObject::tr("Linked directories are not portable: %1").arg(root));
    const QString canonicalRoot = QDir(root).canonicalPath();
    QDirIterator it(root, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        checkCancelled(cancel);
        const auto info = QFileInfo(it.next());
        const QString relative = QDir(root).relativeFilePath(info.absoluteFilePath());
        if (prefix == "minecraft/" && !includedGamePath(relative, options))
            continue;
        require(!info.isSymLink() && info.canonicalFilePath().startsWith(canonicalRoot + '/'),
                QObject::tr("Linked or external file cannot be copied: %1").arg(info.filePath()));
        if (info.isDir())
            continue;
        require(info.isFile(), QObject::tr("Unsupported file: %1").arg(info.filePath()));
        ContentFile file;
        file.path = prefix + relative;
        require(contentPath(file.path) && info.size() <= FileLimit, QObject::tr("Unsupported file path or size: %1").arg(relative));
        file.size = info.size();
        file.sha256 = fileHash(info.filePath(), cancel);
        require(!file.sha256.isEmpty(), QObject::tr("Cannot hash file: %1").arg(info.filePath()));
        files.append(file);
        sources.insert(file.path, info.filePath());
        require(files.size() <= EntryLimit, QObject::tr("Too many files."));
    }
}
Workspace readWorkspace(const QString& root)
{
    require(!QFileInfo(root).isSymLink(), QObject::tr("Linked instances are not supported: %1").arg(root));
    require(!QFileInfo(QDir(root).filePath("instance.cfg")).isSymLink() &&
            !QFileInfo(QDir(root).filePath("mmc-pack.json")).isSymLink(), QObject::tr("Linked instance metadata is not supported."));
    INIFile cfg;
    require(cfg.loadFile(readSmall(QDir(root).filePath("instance.cfg"))), QObject::tr("Cannot read instance.cfg in %1").arg(root));
    require(cfg.get("InstanceType", "OneSix").toString() == "OneSix", QObject::tr("Unsupported instance type in %1").arg(root));
    Workspace workspace;
    workspace.root = QDir(root).canonicalPath();
    workspace.name = cfg.get("name", QFileInfo(root).fileName()).toString();
    workspace.components = readObject(readSmall(QDir(root).filePath("mmc-pack.json")));
    require(workspace.components["components"].isArray(), QObject::tr("Missing Minecraft components in %1").arg(root));
    workspace.gameDirectory = QDir(root).exists(".minecraft") ? ".minecraft" : "minecraft";
    for (const auto& component : workspace.components["components"].toArray()) {
        const auto object = component.toObject();
        if (object["uid"].toString() == "net.minecraft") workspace.minecraftVersion = object["version"].toString();
    }
    const QDir game(QDir(root).filePath(workspace.gameDirectory));
    workspace.worldsPresent = !QDir(game.filePath("saves")).entryList(QDir::Dirs | QDir::NoDotAndDotDot).isEmpty();
    workspace.screenshotsPresent = !QDir(game.filePath("screenshots")).entryList(QDir::Files).isEmpty();
    const auto metadataPath = QDir(root).filePath("pxl-workspace.json");
    if (QFileInfo::exists(metadataPath)) {
        require(!QFileInfo(metadataPath).isSymLink(), QObject::tr("Linked workspace metadata is not supported."));
        workspace.metadata = readObject(readSmall(metadataPath));
    }
    for (const auto& key : PortableSettings) {
        if (cfg.contains(key))
            workspace.settings[key] = QJsonValue::fromVariant(cfg.value(key));
    }
    const QString icon = cfg.get("iconKey", "default").toString();
    if (safeRelativePath(icon) && !icon.contains('/')) {
        const auto iconRoot = QDir(root).absoluteFilePath("../../icons");
        for (const auto& extension : { ".png", ".jpg", ".svg" }) {
            const QString candidate = QDir(iconRoot).filePath(icon + extension);
            if (QFileInfo::exists(candidate) && !QFileInfo(candidate).isSymLink()) {
                workspace.iconPath = candidate;
                break;
            }
        }
    }
    return workspace;
}
void collectWorkspace(Workspace& workspace, const ExportOptions& options, QHash<QString, QString>& sources,
                      std::atomic_bool* cancel)
{
    auto stored = readWorkspace(workspace.root);
    stored.name = workspace.name.isEmpty() ? stored.name : workspace.name;
    stored.group = workspace.group;
    if (!workspace.iconPath.isEmpty())
        stored.iconPath = workspace.iconPath;
    workspace = stored;
    collectTree(QDir(workspace.root).filePath(workspace.gameDirectory), "minecraft/", options, workspace.files, sources, cancel);
    for (const auto& folder : { "patches", "jarmods" })
        collectTree(QDir(workspace.root).filePath(folder), QString(folder) + '/', options, workspace.files, sources, cancel);
    if (!workspace.iconPath.isEmpty()) {
        // Preserve original icon bytes (including SVG); IconList detects the file contents.
        ContentFile icon;
        icon.path = "icon.png";
        icon.sha256 = fileHash(workspace.iconPath, cancel);
        icon.size = QFileInfo(workspace.iconPath).size();
        require(!icon.sha256.isEmpty() && icon.size < ManifestLimit, QObject::tr("Cannot read custom icon."));
        workspace.files.append(icon);
        sources.insert(icon.path, workspace.iconPath);
    }
    if (options.mode != "offline") {
        QHash<QString, Packwiz::V1::Mod> metadataByFile;
        for (const auto& folder : { "mods", "resourcepacks", "shaderpacks" }) {
            const QDir directory(QDir(workspace.root).filePath(workspace.gameDirectory + '/' + folder));
            const QDir index(directory.filePath(".index"));
            for (const auto& entry : index.entryList({ "*.pw.toml" }, QDir::Files)) {
                checkCancelled(cancel);
                auto metadata = Packwiz::V1::getIndexForMod(index, entry);
                if (metadata.isValid() && safeRelativePath(metadata.filename) && !metadata.filename.contains('/'))
                    metadataByFile.insert(QString(folder) + '/' + metadata.filename, metadata);
            }
        }
        for (auto& file : workspace.files) {
            checkCancelled(cancel);
            const auto source = sources.value(file.path);
            const auto folder = file.path.section('/', 1, 1);
            if (!QStringList{ "mods", "resourcepacks", "shaderpacks" }.contains(folder) || !file.path.startsWith("minecraft/"))
                continue;
            QString key = file.path.mid(QString("minecraft/").size());
            if (key.endsWith(".disabled")) key.chop(9);
            // Existing Packwiz metadata, not a second project-identification system.
            if (!metadataByFile.contains(key)) continue;
            auto metadata = metadataByFile.value(key);
            file.source = metadata.provider == ModPlatform::ResourceProvider::MODRINTH ? "modrinth" : "curseforge";
            file.projectId = metadata.project_id.toString();
            file.versionId = metadata.file_id.toString();
            file.side = metadata.side == ModPlatform::Side::ClientSide ? "client" :
                        metadata.side == ModPlatform::Side::ServerSide ? "server" :
                        metadata.side == ModPlatform::Side::UniversalSide ? "both" : "unknown";
            if (file.versionId.isEmpty() || !trustedDownload(metadata.url.toString()))
                continue;
            auto algorithm = QCryptographicHash::Sha256;
            if (metadata.hash_format == "sha1") algorithm = QCryptographicHash::Sha1;
            else if (metadata.hash_format == "sha512") algorithm = QCryptographicHash::Sha512;
            else if (metadata.hash_format != "sha256") continue;
            QFile input(source);
            QCryptographicHash hash(algorithm);
            if (!input.open(QIODevice::ReadOnly) || !hash.addData(&input) || hash.result().toHex() != metadata.hash.toLatin1().toLower())
                continue;  // Modified local files are always bundled.
            file.url = metadata.url.toString();
            file.bundled = false;
        }
    }
}
}  // namespace

bool safeRelativePath(const QString& path)
{
    if (path.isEmpty() || path.size() > 1024 || path.startsWith('/') || path.contains('\\') || path.contains(':') ||
        path != path.normalized(QString::NormalizationForm_C))
        return false;
    static const QRegularExpression forbidden(R"([\x00-\x1f<>"|?*])");
    static const QRegularExpression reserved(R"(^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\.|$))", QRegularExpression::CaseInsensitiveOption);
    if (forbidden.match(path).hasMatch())
        return false;
    for (const auto& part : path.split('/')) {
        if (part.isEmpty() || part == "." || part == ".." || part.endsWith('.') || part.endsWith(' ') || reserved.match(part).hasMatch())
            return false;
    }
    return true;
}

QString fileHash(const QString& path, std::atomic_bool* cancel)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    QByteArray buffer(256 * 1024, Qt::Uninitialized);
    while (!file.atEnd()) {
        if (cancel && cancel->load()) return {};
        const auto count = file.read(buffer.data(), buffer.size());
        if (count <= 0) return {};
        hash.addData(QByteArrayView(buffer.constData(), count));
    }
    return QString::fromLatin1(hash.result().toHex());
}

QJsonObject toJson(const Manifest& manifest)
{
    QJsonArray instances;
    for (const auto& workspace : manifest.instances) {
        QJsonArray files;
        for (const auto& file : workspace.files) {
            files.append(QJsonObject{ { "path", file.path }, { "sha256", file.sha256 }, { "size", file.size },
                { "source", file.source }, { "projectId", file.projectId }, { "versionId", file.versionId },
                { "url", file.url }, { "side", file.side }, { "bundled", file.bundled } });
        }
        instances.append(QJsonObject{ { "name", workspace.name }, { "group", workspace.group },
                                      { "components", workspace.components }, { "settings", workspace.settings },
                                      { "metadata", workspace.metadata }, { "files", files } });
    }
    return { { "formatVersion", 1 }, { "format", manifest.format }, { "mode", manifest.mode }, { "instances", instances } };
}

QString fromJson(const QJsonObject& object, Manifest& manifest)
{
    try {
        Manifest result;
        require(object["formatVersion"].toDouble() == 1, QObject::tr("Unsupported PXL archive version."));
        require(!object.contains("requires") || (object["requires"].isArray() && object["requires"].toArray().isEmpty()),
                QObject::tr("This archive requires unsupported capabilities."));
        result.format = object["format"].toString();
        result.mode = object["mode"].toString();
        require((result.format == "pxlconep" && (result.mode == "smart" || result.mode == "offline")) ||
                (result.format == "pxlt" && (result.mode == "thin" || result.mode == "offline")), QObject::tr("Invalid PXL archive format or mode."));
        require(object["instances"].isArray(), QObject::tr("Missing instances."));
        const auto instances = object["instances"].toArray();
        require(!instances.isEmpty() && instances.size() <= 1000 && (result.format != "pxlconep" || instances.size() == 1),
                QObject::tr("Invalid number of instances."));
        qint64 total = 0;
        int count = 0;
        for (const auto& value : instances) {
            require(value.isObject(), QObject::tr("Invalid instance."));
            auto obj = value.toObject();
            Workspace workspace;
            workspace.name = obj["name"].toString();
            workspace.group = obj["group"].toString();
            workspace.components = obj["components"].toObject();
            workspace.settings = obj["settings"].toObject();
            require(!obj.contains("metadata") || obj["metadata"].isObject(), QObject::tr("Invalid workspace metadata."));
            workspace.metadata = obj["metadata"].toObject();
            workspace.gameDirectory = "minecraft";
            for (const auto& component : workspace.components["components"].toArray()) {
                const auto entry = component.toObject();
                if (entry["uid"].toString() == "net.minecraft") workspace.minecraftVersion = entry["version"].toString();
            }
            require(!workspace.name.trimmed().isEmpty() && workspace.name.size() <= 256 && workspace.group.size() <= 256,
                    QObject::tr("Invalid instance name or group."));
            const auto components = workspace.components["components"].toArray();
            require(workspace.components["formatVersion"].toInt() == 1 && !components.isEmpty(), QObject::tr("Missing Minecraft version information."));
            bool minecraft = false;
            for (const auto& component : components) {
                const auto uid = component.toObject()["uid"].toString();
                const auto version = component.toObject()["version"].toString();
                require(safeRelativePath(uid) && !uid.contains('/') && safeRelativePath(version) && !version.contains('/'),
                        QObject::tr("Invalid component identity."));
                minecraft |= uid == "net.minecraft";
            }
            require(minecraft, QObject::tr("Missing Minecraft component."));
            require(obj["files"].isArray(), QObject::tr("Missing content list."));
            PathSet paths;
            for (const auto& entry : obj["files"].toArray()) {
                require(entry.isObject(), QObject::tr("Invalid content entry."));
                const auto item = entry.toObject();
                ContentFile file;
                file.path = item["path"].toString();
                file.sha256 = item["sha256"].toString();
                const auto size = item["size"].toDouble(-1);
                require(size >= 0 && size <= FileLimit && size == static_cast<qint64>(size), QObject::tr("Invalid content size."));
                file.size = static_cast<qint64>(size);
                file.source = item["source"].toString();
                file.projectId = item["projectId"].toString();
                file.versionId = item["versionId"].toString();
                file.url = item["url"].toString();
                file.side = item["side"].toString();
                require(item["bundled"].isBool(), QObject::tr("Missing content storage mode."));
                file.bundled = item["bundled"].toBool();
                static const QRegularExpression checksum("^[0-9a-f]{64}$");
                require(contentPath(file.path) && checksum.match(file.sha256).hasMatch(), QObject::tr("Unsafe path or invalid hash: %1").arg(file.path));
                require(QStringList{ "client", "server", "both", "unknown" }.contains(file.side), QObject::tr("Invalid content side."));
                require(file.bundled || (result.mode != "offline" && trustedDownload(file.url) && !file.projectId.isEmpty() &&
                    !file.versionId.isEmpty() && QStringList{ "modrinth", "curseforge" }.contains(file.source)),
                    QObject::tr("Unresolvable or unsafe content reference: %1").arg(file.path));
                addPath(paths, file.path);
                total += file.size;
                require(total <= TotalLimit && ++count <= EntryLimit, QObject::tr("Archive exceeds size or file-count limits."));
                workspace.files.append(file);
                workspace.worldsPresent |= file.path.startsWith("minecraft/saves/");
                workspace.screenshotsPresent |= file.path.startsWith("minecraft/screenshots/");
            }
            result.instances.append(workspace);
        }
        manifest = result;
        return {};
    } catch (const Failure& failure) { return failure.message; }
}

QString inspectArchive(const QString& archive, Manifest& manifest, std::atomic_bool* cancel, bool verifyPayloads)
{
    try {
        MMCZip::ArchiveReader reader(archive);
        QByteArray bytes;
        PathSet paths;
        QMap<QString, qint64> sizes;
        QMap<QString, QString> hashes;
        qint64 total = 0;
        int entries = 0;
        require(reader.parse([&](MMCZip::ArchiveReader::File* entry) {
            checkCancelled(cancel);
            QString path = entry->filename();
            if (entry->isDirectory() && path.endsWith('/')) path.chop(1);
            require(safeRelativePath(path) && !entry->isLink() && (entry->isFile() || entry->isDirectory()),
                    QObject::tr("Unsafe archive entry: %1").arg(path));
            require(++entries <= EntryLimit, QObject::tr("Too many archive entries."));
            addPath(paths, path, entry->isDirectory());
            if (entry->isDirectory()) return true;
            require(entry->size() >= 0 && entry->size() <= FileLimit, QObject::tr("Archive entry too large."));
            total += entry->size();
            require(total <= TotalLimit, QObject::tr("Archive exceeds size limit."));
            sizes.insert(path, entry->size());
            if (path == "manifest.json") {
                require(entry->size() <= ManifestLimit, QObject::tr("Manifest exceeds size limit."));
                QByteArray buffer(64 * 1024, Qt::Uninitialized);
                qint64 count = 0;
                while ((count = entry->read(buffer.data(), buffer.size())) > 0) {
                    checkCancelled(cancel);
                    require(bytes.size() + count <= ManifestLimit && bytes.size() + count <= entry->size(),
                            QObject::tr("Manifest exceeds its declared size."));
                    bytes.append(buffer.constData(), count);
                }
                require(count == 0 && bytes.size() == entry->size(), QObject::tr("Truncated manifest."));
            } else if (verifyPayloads) {
                QCryptographicHash hash(QCryptographicHash::Sha256);
                QByteArray buffer(256 * 1024, Qt::Uninitialized);
                qint64 count = 0, size = 0;
                while ((count = entry->read(buffer.data(), buffer.size())) > 0) {
                    checkCancelled(cancel);
                    size += count;
                    require(size <= entry->size(), QObject::tr("Payload exceeds declared size."));
                    hash.addData(QByteArrayView(buffer.constData(), count));
                }
                require(count == 0 && size == entry->size(), QObject::tr("Truncated payload: %1").arg(path));
                hashes[path] = QString::fromLatin1(hash.result().toHex());
            }
            return true;
        }), QObject::tr("Cannot read PXL archive."));
        require(!bytes.isEmpty(), QObject::tr("Archive has no PXL manifest."));
        Manifest parsed;
        const auto error = fromJson(readObject(bytes), parsed);
        require(error.isEmpty(), error);
        QSet<QString> expected{ "manifest.json" };
        for (int i = 0; i < parsed.instances.size(); ++i) {
            for (const auto& file : parsed.instances[i].files) {
                if (!file.bundled) continue;
                const QString path = QString("instances/%1/%2").arg(i).arg(file.path);
                require(sizes.contains(path) && sizes.value(path) == file.size, QObject::tr("Missing or incorrect payload: %1").arg(path));
                require(!verifyPayloads || hashes.value(path) == file.sha256, QObject::tr("Checksum mismatch: %1").arg(path));
                expected.insert(path);
            }
        }
        require(expected.size() == sizes.size(), QObject::tr("Archive contains undeclared files."));
        manifest = parsed;
        return {};
    } catch (const Failure& failure) { return failure.message; }
}

QString exportArchive(const QString& output, const QVector<Workspace>& instances, const ExportOptions& options,
                      std::atomic_bool* cancel, Progress progress)
{
    try {
        checkCancelled(cancel);
        Manifest manifest{ options.format, options.mode, {} };
        QVector<QHash<QString, QString>> sources;
        for (auto workspace : instances) {
            const auto sourceRoot = QDir(workspace.root).canonicalPath();
            const auto destination = QFileInfo(output).absoluteFilePath();
            require(!sourceRoot.isEmpty() && !destination.startsWith(sourceRoot + '/', Qt::CaseInsensitive),
                    QObject::tr("Save the archive outside the source instance."));
            if (progress) progress(manifest.instances.size(), instances.size(), QObject::tr("Reading %1").arg(workspace.name));
            QHash<QString, QString> paths;
            collectWorkspace(workspace, options, paths, cancel);
            manifest.instances.append(workspace);
            sources.append(paths);
        }
        Manifest validated;
        const auto error = fromJson(toJson(manifest), validated);
        require(error.isEmpty(), error);
        QTemporaryFile temporary(QFileInfo(output).absolutePath() + "/.pxl-export-XXXXXX");
        require(temporary.open(), QObject::tr("Cannot create export temporary file."));
        const QString path = temporary.fileName();
        temporary.close();
        MMCZip::ArchiveWriter writer(path);
        require(writer.open(), QObject::tr("Cannot create archive."));
        const auto manifestBytes = QJsonDocument(toJson(manifest)).toJson();
        require(manifestBytes.size() <= ManifestLimit && writer.addFile("manifest.json", manifestBytes), QObject::tr("Cannot write manifest."));
        for (int i = 0; i < manifest.instances.size(); ++i) {
            const auto& workspace = manifest.instances[i];
            qint64 completed = 0;
            for (const auto& file : workspace.files) {
                checkCancelled(cancel);
                if (progress) progress(completed++, workspace.files.size(), file.path);
                const auto source = sources[i].value(file.path);
                require(fileHash(source, cancel) == file.sha256, QObject::tr("File changed during export: %1").arg(file.path));
                if (file.bundled)
                    require(writer.addFile(source, QString("instances/%1/%2").arg(i).arg(file.path)), QObject::tr("Cannot archive %1").arg(file.path));
            }
        }
        require(writer.close(), QObject::tr("Cannot finish archive."));
        Manifest check;
        const auto verificationError = inspectArchive(path, check, cancel, true);
        require(verificationError.isEmpty(), QObject::tr("Export verification failed: %1").arg(verificationError));
        QSaveFile destination(output);
        QFile source(path);
        require(source.open(QIODevice::ReadOnly) && destination.open(QIODevice::WriteOnly), QObject::tr("Cannot save export."));
        while (!source.atEnd()) {
            checkCancelled(cancel);
            auto bytes = source.read(256 * 1024);
            require(!bytes.isEmpty() && destination.write(bytes) == bytes.size(), QObject::tr("Cannot write export."));
        }
        checkCancelled(cancel);
        require(destination.commit(), QObject::tr("Cannot commit export."));
        return {};
    } catch (const Failure& failure) { return failure.message; }
}

QString prepareArchiveInstance(const QString& archive, int index, const QString& staging, Workspace& prepared,
                               std::atomic_bool* cancel, Progress progress)
{
    try {
        Manifest manifest;
        const auto error = inspectArchive(archive, manifest, cancel);
        require(error.isEmpty(), error);
        require(index >= 0 && index < manifest.instances.size(), QObject::tr("Invalid selected instance."));
        require(QDir(staging).exists() && QDir(staging).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden).isEmpty(),
                QObject::tr("Import staging directory must be empty."));
        prepared = manifest.instances[index];
        prepared.root = staging;
        QHash<QString, ContentFile> payload;
        for (const auto& file : prepared.files) {
            if (file.bundled) payload.insert(QString("instances/%1/%2").arg(index).arg(file.path), file);
            require(QDir().mkpath(QFileInfo(QDir(staging).filePath(file.path)).absolutePath()), QObject::tr("Cannot create content directory."));
        }
        MMCZip::ArchiveReader reader(archive);
        qint64 completed = 0;
        require(reader.parse([&](MMCZip::ArchiveReader::File* entry) {
            checkCancelled(cancel);
            if (!payload.contains(entry->filename())) return true;
            const auto file = payload.take(entry->filename());
            require(entry->isFile() && !entry->isLink() && entry->size() == file.size, QObject::tr("Archive changed during import."));
            QSaveFile destination(QDir(staging).filePath(file.path));
            require(destination.open(QIODevice::WriteOnly), QObject::tr("Cannot extract %1").arg(file.path));
            QCryptographicHash hash(QCryptographicHash::Sha256);
            QByteArray buffer(256 * 1024, Qt::Uninitialized);
            qint64 size = 0, count = 0;
            while ((count = entry->read(buffer.data(), buffer.size())) > 0) {
                checkCancelled(cancel);
                size += count;
                require(size <= file.size && destination.write(buffer.constData(), count) == count, QObject::tr("Invalid or unwritable payload."));
                hash.addData(QByteArrayView(buffer.constData(), count));
            }
            require(count == 0 && size == file.size && hash.result().toHex() == file.sha256.toLatin1(),
                    QObject::tr("Checksum mismatch: %1").arg(file.path));
            require(destination.commit(), QObject::tr("Cannot finish extracting %1").arg(file.path));
            if (progress) progress(++completed, prepared.files.size(), file.path);
            return true;
        }), QObject::tr("Cannot extract archive."));
        require(payload.isEmpty(), QObject::tr("Archive changed or payload is missing."));
        checkCancelled(cancel);
        writeMetadata(prepared, staging);
        return {};
    } catch (const Failure& failure) { return failure.message; }
}

QString verifyInstance(const Workspace& workspace, const QString& staging, std::atomic_bool* cancel)
{
    for (const auto& file : workspace.files) {
        if (cancel && cancel->load()) return QObject::tr("Cancelled.");
        const QString path = QDir(staging).filePath(file.path);
        if (QFileInfo(path).size() != file.size || fileHash(path, cancel) != file.sha256)
            return QObject::tr("Required content is missing or has an incorrect checksum: %1").arg(file.path);
    }
    return {};
}

QString scanLauncher(const QString& directory, QVector<Workspace>& instances, std::atomic_bool* cancel)
{
    try {
        instances.clear();
        QDir root(directory);
        if (root.exists("instances")) root.cd("instances");
        require(root.exists() && !QFileInfo(root.absolutePath()).isSymLink(), QObject::tr("Select a launcher or instances directory."));
        QJsonObject groups;
        const QString groupsPath = root.filePath("instgroups.json");
        if (QFileInfo::exists(groupsPath)) groups = readObject(readSmall(groupsPath))["groups"].toObject();
        const auto folders = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const auto& folder : folders) {
            checkCancelled(cancel);
            if (folder.isSymLink() || !QFileInfo::exists(QDir(folder.filePath()).filePath("instance.cfg"))) continue;
            try {
                auto workspace = readWorkspace(folder.filePath());
                for (auto group = groups.begin(); group != groups.end(); ++group) {
                    const auto members = group.value().toObject()["instances"].toArray();
                    if (members.contains(folder.fileName())) workspace.group = group.key();
                }
                instances.append(workspace);
            } catch (const Failure&) {
                // Show unsupported/corrupt instances in the preview, with no components.
                Workspace unsupported;
                unsupported.root = folder.absoluteFilePath();
                unsupported.name = folder.fileName();
                instances.append(unsupported);
            }
        }
        require(!instances.isEmpty(), QObject::tr("No Prism/Pinecone instances found in this directory."));
        return {};
    } catch (const Failure& failure) { return failure.message; }
}

QString copyLauncherInstance(const Workspace& source, const QString& staging, std::atomic_bool* cancel, Progress progress)
{
    try {
        const auto src = QDir(source.root).canonicalPath();
        const auto dst = QDir(staging).canonicalPath();
        require(!src.isEmpty() && !dst.isEmpty() && src.compare(dst, Qt::CaseInsensitive) != 0 &&
                !dst.startsWith(src + '/', Qt::CaseInsensitive) && !src.startsWith(dst + '/', Qt::CaseInsensitive),
                QObject::tr("Source and destination must not overlap."));
        require(QDir(staging).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden).isEmpty(), QObject::tr("Staging directory must be empty."));
        Workspace workspace = source;
        QHash<QString, QString> sources;
        collectWorkspace(workspace, { "pxlt", "offline", true, true }, sources, cancel);
        qint64 completed = 0;
        for (const auto& file : workspace.files) {
            checkCancelled(cancel);
            const auto destination = QDir(staging).filePath(file.path);
            require(QDir().mkpath(QFileInfo(destination).absolutePath()), QObject::tr("Cannot create content directory."));
            QFile input(sources.value(file.path));
            QSaveFile output(destination);
            require(input.open(QIODevice::ReadOnly) && output.open(QIODevice::WriteOnly), QObject::tr("Cannot copy %1").arg(file.path));
            QCryptographicHash hash(QCryptographicHash::Sha256);
            qint64 copied = 0;
            while (!input.atEnd()) {
                checkCancelled(cancel);
                const auto bytes = input.read(256 * 1024);
                copied += bytes.size();
                require(!bytes.isEmpty() && copied <= file.size && output.write(bytes) == bytes.size(), QObject::tr("Cannot copy %1").arg(file.path));
                hash.addData(bytes);
            }
            require(copied == file.size && hash.result().toHex() == file.sha256.toLatin1(), QObject::tr("File changed while copying: %1").arg(file.path));
            require(output.commit(), QObject::tr("Cannot finish copying %1").arg(file.path));
            if (progress) progress(++completed, workspace.files.size(), file.path);
        }
        writeMetadata(workspace, staging);
        return {};
    } catch (const Failure& failure) { return failure.message; }
}

QStringList launcherLocations()
{
    QStringList result;
    const auto home = QDir::homePath();
    QStringList roots = { QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation), home + "/.local/share",
        home + "/.var/app/org.prismlauncher.PrismLauncher/data", home + "/.var/app/ru.pineconemc.PineconeMC/data" };
    for (const auto& root : roots) {
        for (const auto& name : { "PrismLauncher", "PineconeMC", "pineconemc" }) {
            const QString path = QDir(root).filePath(name);
            if (QDir(path).exists("instances")) result.append(QDir(path).canonicalPath());
        }
    }
    result.removeDuplicates();
    return result;
}
}  // namespace Pxl

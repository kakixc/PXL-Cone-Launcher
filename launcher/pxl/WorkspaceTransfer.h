// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QJsonObject>
#include <QStringList>
#include <QVector>
#include <atomic>
#include <functional>

namespace Pxl {

struct ContentFile {
    QString path;
    QString sha256;
    qint64 size = 0;
    QString source = "local";
    QString projectId;
    QString versionId;
    QString url;
    QString side = "unknown";
    bool bundled = true;
};

struct Workspace {
    QString name;
    QString group;
    QString root;  // Local source directory; never serialized.
    QString gameDirectory;
    QJsonObject components;
    QJsonObject settings;
    QJsonObject metadata;
    QVector<ContentFile> files;
    QString iconPath;
    QString minecraftVersion;  // Local preview information; not part of the manifest schema.
    bool worldsPresent = false;
    bool screenshotsPresent = false;
};

struct Manifest {
    QString format;  // pxlconep or pxlt
    QString mode;    // smart, thin or offline
    QVector<Workspace> instances;
};

struct ExportOptions {
    QString format = "pxlconep";
    QString mode = "smart";
    bool worlds = false;
    bool screenshots = false;
};

using Progress = std::function<void(qint64, qint64, const QString&)>;

// An empty QString denotes success. All paths written by these functions must
// belong to an operation-owned staging directory, not an existing instance.
bool safeRelativePath(const QString& path);
QJsonObject toJson(const Manifest& manifest);
QString fromJson(const QJsonObject& object, Manifest& manifest);
QString inspectArchive(const QString& archive, Manifest& manifest, std::atomic_bool* cancel = nullptr, bool verifyPayloads = false);
QString exportArchive(const QString& output, const QVector<Workspace>& instances, const ExportOptions& options,
                      std::atomic_bool* cancel = nullptr, Progress progress = {});
QString prepareArchiveInstance(const QString& archive, int index, const QString& staging, Workspace& prepared,
                               std::atomic_bool* cancel = nullptr, Progress progress = {});
QString verifyInstance(const Workspace& workspace, const QString& staging, std::atomic_bool* cancel = nullptr);
QString scanLauncher(const QString& directory, QVector<Workspace>& instances, std::atomic_bool* cancel = nullptr);
QString copyLauncherInstance(const Workspace& source, const QString& staging, std::atomic_bool* cancel = nullptr,
                             Progress progress = {});
QStringList launcherLocations();
QString fileHash(const QString& file, std::atomic_bool* cancel = nullptr);
}  // namespace Pxl

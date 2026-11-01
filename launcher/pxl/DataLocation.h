// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QString>

namespace Pxl {
inline QString dataDirectoryOverride(const QProcessEnvironment& environment, const QString& legacyName)
{
    const auto pxl = environment.value(QStringLiteral("PXLCONE_DATA_DIR"));
    return pxl.isEmpty() ? environment.value(legacyName.toUpper() + QStringLiteral("_DATA_DIR")) : pxl;
}

struct DataLocation {
    QString path;
    QString reason;
    bool portable = false;
};

// Select only: never move, create or implicitly adopt another launcher's data.
inline DataLocation selectDataLocation(const QString& commandLine, const QString& environment,
                                       const QString& root, const QString& persistent, bool allowPortable)
{
    if (!commandLine.isEmpty())
        return { commandLine, QStringLiteral("Command line") };
    if (!environment.isEmpty())
        return { environment, QStringLiteral("System environment") };
    if (allowPortable) {
        const auto userData = QDir(root).filePath(QStringLiteral("UserData"));
        if (QDir(userData).exists())
            return { userData, QStringLiteral("Portable user data path"), true };
        if (QFileInfo::exists(QDir(root).filePath(QStringLiteral("portable.txt"))))
            return { root, QStringLiteral("Portable data path"), true };
    }
    return { persistent, QStringLiteral("Persistent data path") };
}
}  // namespace Pxl

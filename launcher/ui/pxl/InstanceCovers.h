// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "Backdrop.h"
#include <QCache>
#include <QDir>
#include <QSaveFile>

namespace Pxl {
inline QString instanceCoverPath(const QString& root) { return QDir(root).filePath(".pxl/cover.png"); }
inline QCache<QString, QPixmap>& coverCache() { static QCache<QString, QPixmap> cache(24 * 1024); return cache; }
inline QPixmap instanceCover(const QString& root)
{
    const QFileInfo file(instanceCoverPath(root));
    if (!file.isFile() || file.isSymLink() || QFileInfo(file.absolutePath()).isSymLink()) return {};
    const auto key = file.absoluteFilePath() + QString::number(file.lastModified().toMSecsSinceEpoch()) + QString::number(file.size());
    if (auto* cached = coverCache().object(key)) return *cached;
    auto image = loadBackdropImage(file.absoluteFilePath());
    if (image.isNull()) return {};
    auto pixmap = QPixmap::fromImage(image.scaled(720, 400, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    coverCache().insert(key, new QPixmap(pixmap), qMax(1, pixmap.width() * pixmap.height() * 4 / 1024));
    return pixmap;
}
inline bool writeInstanceCover(const QString& root, const QImage& image, QString* error)
{
    auto fail = [error](const QString& message) { if (error) *error = message; return false; };
    if (image.isNull()) return fail(QObject::tr("Choose a valid image up to 16384 pixels and 64 megapixels."));
    const auto path = instanceCoverPath(root);
    const QFileInfo target(path);
    if (QFileInfo(target.absolutePath()).isSymLink() || target.isSymLink())
        return fail(QObject::tr("The cover destination must not be a symbolic link."));
    if (!QDir().mkpath(target.absolutePath())) return fail(QObject::tr("Could not create the cover folder."));
    if (QFileInfo(target.absolutePath()).canonicalFilePath() != QDir(QFileInfo(root).canonicalFilePath()).filePath(".pxl"))
        return fail(QObject::tr("The cover destination must not be a symbolic link."));
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || !image.save(&file, "PNG") || !file.commit())
        return fail(QObject::tr("Could not save the cover. The previous cover has been kept."));
    coverCache().clear();
    return true;
}
inline bool saveInstanceCover(const QString& root, const QString& source, QString* error)
{
    return writeInstanceCover(root, loadBackdropImage(source), error);
}
inline bool setInstanceCoverColor(const QString& root, const QColor& color, QString* error)
{
    QImage image(720, 280, QImage::Format_RGB32);
    image.fill(color);
    QPainter painter(&image);
    QLinearGradient shade(0,0,720,280);
    shade.setColorAt(0,QColor(255,255,255,14));
    shade.setColorAt(1,QColor(0,0,0,32));
    painter.fillRect(image.rect(),shade);
    painter.end();
    return writeInstanceCover(root,image,error);
}
inline bool resetInstanceCover(const QString& root)
{
    const QFileInfo file(instanceCoverPath(root));
    if (file.isSymLink() || QFileInfo(file.absolutePath()).isSymLink()) return false;
    const bool ok = !file.exists() || QFile::remove(file.absoluteFilePath());
    if (ok) coverCache().clear();
    return ok;
}
} // namespace Pxl

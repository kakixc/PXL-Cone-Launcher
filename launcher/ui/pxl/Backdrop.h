// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QGraphicsBlurEffect>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QImageReader>
#include <QFileInfo>
#include <QDateTime>

namespace Pxl {
inline QImage loadBackdropImage(const QString& path)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const auto dimensions = reader.size();
    if (!dimensions.isValid() || dimensions.width() > 16384 || dimensions.height() > 16384 ||
        qint64(dimensions.width()) * dimensions.height() > 64000000) return {};
    reader.setScaledSize(dimensions.scaled(1600, 1000, Qt::KeepAspectRatio));
    return reader.read();
}

inline QPixmap blurredBackdrop(const QString& path = {})
{
    const QFileInfo file(path);
    const QString key = path + QString::number(file.lastModified().toMSecsSinceEpoch()) + QString::number(file.size());
    static QString cachedKey;
    static QPixmap cached;
    if (cached.isNull() || cachedKey != key) {
        QImage source = path.isEmpty() ? QImage() : loadBackdropImage(path);
        if (source.isNull()) source.load(":/pxl/wallpaper-source.png");
        if (source.isNull()) return QPixmap();
        const auto reduced = source.scaled(1200, 800, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QGraphicsScene scene;
        auto* item = scene.addPixmap(QPixmap::fromImage(reduced));
        auto* blur = new QGraphicsBlurEffect;
        blur->setBlurRadius(22);
        blur->setBlurHints(QGraphicsBlurEffect::QualityHint);
        item->setGraphicsEffect(blur);
        QPixmap result(reduced.size());
        result.fill(Qt::transparent);
        QPainter painter(&result);
        scene.render(&painter, QRectF(result.rect()), QRectF(reduced.rect()));
        painter.end();
        cached = result;
        cachedKey = key;
    }
    return cached;
}

inline void paintBackdrop(QPainter& painter, const QRect& bounds, const QPixmap& image, int shade)
{
    if (image.isNull()) return;
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    const auto scale = qMax(qreal(bounds.width()) / image.width(), qreal(bounds.height()) / image.height());
    const QSizeF size(image.width() * scale, image.height() * scale);
    painter.drawPixmap(QRectF(bounds.center().x() - size.width() / 2, bounds.center().y() - size.height() / 2,
                              size.width(), size.height()), image, QRectF(image.rect()));
    painter.fillRect(bounds, QColor(14, 18, 23, shade));
}
} // namespace Pxl

// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QGraphicsBlurEffect>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QWidget>

namespace Pxl {
// Capture only our own application widget, including padding so blur has no dark edge.
inline QPixmap frostedBackdrop(QWidget* source, const QRect& globalArea)
{
    if (!source || globalArea.isEmpty()) return {};
    auto* window = source->window();
    const QRect requested(window->mapFromGlobal(globalArea.topLeft()), globalArea.size());
    const QRect captureArea = requested.adjusted(-24, -24, 24, 24).intersected(window->rect());
    if (captureArea.isEmpty()) return {};
    const auto snapshot = window->grab(captureArea);
    QGraphicsScene scene;
    auto* item = scene.addPixmap(snapshot);
    auto* blur = new QGraphicsBlurEffect;
    blur->setBlurRadius(22);
    blur->setBlurHints(QGraphicsBlurEffect::PerformanceHint);
    item->setGraphicsEffect(blur);
    const auto ratio = snapshot.devicePixelRatio();
    QPixmap result(globalArea.size() * ratio);
    result.setDevicePixelRatio(ratio);
    result.fill(QColor("#202328"));
    QPainter painter(&result);
    scene.render(&painter, QRectF(QPointF(), globalArea.size()),
                 QRectF(requested.topLeft() - captureArea.topLeft(), requested.size()));
    return result;
}

inline void paintFrostedPanel(QPainter& painter, const QRectF& area, const QPixmap& backdrop, qreal radius = 14)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath clip;
    clip.addRoundedRect(area, radius, radius);
    painter.setClipPath(clip);
    if (!backdrop.isNull()) painter.drawPixmap(area, backdrop, QRectF(backdrop.rect()));
    painter.fillRect(area, QColor(30, 33, 39, 205));
    painter.setClipping(false);
    painter.setPen(QColor(210, 219, 232, 42));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(area.adjusted(.5, .5, -.5, -.5), radius, radius);
    painter.restore();
}

class FrostedMenu final : public QMenu {
public:
    explicit FrostedMenu(QWidget* parent) : QMenu(parent), m_source(parent)
    {
        setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
        setAttribute(Qt::WA_TranslucentBackground);
        setStyleSheet("QMenu { background: transparent; border: none; padding: 8px; }"
                      "QMenu::item { padding: 9px 22px; border-radius: 7px; }"
                      "QMenu::item:selected { background: rgba(255,255,255,24); color: #f2f3f5; }");
    }
protected:
    void showEvent(QShowEvent* event) override
    {
        m_backdrop = frostedBackdrop(m_source, QRect(mapToGlobal(QPoint()), size()));
        QMenu::showEvent(event);
    }
    void paintEvent(QPaintEvent* event) override
    {
        { QPainter painter(this); paintFrostedPanel(painter, rect(), m_backdrop); }
        QMenu::paintEvent(event);
    }
private:
    QWidget* m_source;
    QPixmap m_backdrop;
};
} // namespace Pxl

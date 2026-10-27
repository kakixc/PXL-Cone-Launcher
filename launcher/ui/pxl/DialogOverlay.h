// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QDialog>
#include <QEvent>
#include <QEventLoop>
#include <QPainter>
#include <QPointer>
#include <QTimer>
#include <QVariant>
#include <QPropertyAnimation>
#include <QPainterPath>
#include "Animations.h"
#include "FrostedSurface.h"

namespace Pxl {
// A real child surface: no native window, taskbar entry or independent geometry.
class DialogOverlay final : public QWidget {
public:
    DialogOverlay(QDialog* dialog, QWidget* surface) : QWidget(surface), m_dialog(dialog)
    {
        setObjectName("pxlDialogOverlay");
        setGeometry(surface->rect());
        m_backdrop = frostedBackdrop(surface, QRect(surface->mapToGlobal(QPoint()), surface->size()));
        surface->installEventFilter(this);
        dialog->installEventFilter(this);
        dialog->setParent(this, Qt::Widget);
        dialog->setWindowModality(Qt::NonModal);
        m_originalStyle = dialog->styleSheet();
        dialog->setStyleSheet(m_originalStyle + "\nQDialog { background: transparent; border: none; }");
        dialog->setMinimumSize(0, 0);
        m_transition = new QPropertyAnimation(dialog, "geometry", this);
        m_transition->setDuration(160);
        m_transition->setEasingCurve(QEasingCurve::OutCubic);
        arrange();
    }
    ~DialogOverlay() override
    {
        // The caller may own the dialog on its stack. Never let a disappearing
        // surface delete that dialog through QObject parent ownership.
        if (m_dialog) {
            m_dialog->hide();
            m_dialog->setStyleSheet(m_originalStyle);
            m_dialog->setParent(nullptr, Qt::Dialog);
        }
    }

    static int run(QDialog* dialog, QWidget* surface)
    {
        const QPointer<QWidget> originalParent = dialog->parentWidget();
        const QPointer<QDialog> guardedDialog = dialog;
        const auto originalFlags = dialog->windowFlags();
        const auto originalMask = dialog->mask();
        const auto focused = QPointer<QWidget>(surface->window()->focusWidget());
        QPointer<DialogOverlay> overlay = new DialogOverlay(dialog, surface);
        QList<QPointer<QWidget>> disabled;
        for (auto* sibling : surface->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly)) {
            if (sibling != overlay.data() && sibling->isEnabled()) {
                sibling->setEnabled(false);
                disabled.append(sibling);
            }
        }
        QEventLoop loop;
        QObject::connect(dialog, &QDialog::finished, &loop, &QEventLoop::quit);
        QObject::connect(dialog, &QObject::destroyed, &loop, &QEventLoop::quit);
        QObject::connect(overlay.data(), &QObject::destroyed, &loop, &QEventLoop::quit);
        QObject::connect(surface, &QObject::destroyed, &loop, &QEventLoop::quit);
        overlay->show();
        overlay->raise();
        dialog->show();
        // Animate geometry, not an opacity snapshot of a composite scroll view.
        // Cached effects can retain pixels from the underlying workspace.
        if (QApplication::isEffectEnabled(Qt::UI_AnimateMenu)) {
            const auto target = dialog->geometry();
            dialog->move(target.topLeft() + QPoint(0, 6));
            overlay->m_transition->setStartValue(dialog->geometry());
            overlay->m_transition->setEndValue(target);
            overlay->m_transition->start();
        }
        dialog->setFocus(Qt::OtherFocusReason);
        loop.exec();
        const int result = guardedDialog ? guardedDialog->result() : QDialog::Rejected;
        if (guardedDialog) {
            guardedDialog->hide();
            if (overlay) guardedDialog->setStyleSheet(overlay->m_originalStyle);
            guardedDialog->setMask(originalMask);
            guardedDialog->setParent(originalParent.data(), originalFlags);
        }
        // It has already been detached above; do not detach it again in cleanup.
        if (overlay) {
            overlay->m_dialog.clear();
            delete overlay.data();
        }
        for (const auto& sibling : disabled) if (sibling) sibling->setEnabled(true);
        if (focused) focused->setFocus(Qt::OtherFocusReason);
        return result;
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == parentWidget() && event->type() == QEvent::Resize) {
            setGeometry(parentWidget()->rect());
            arrange();
        } else if (watched == parentWidget() && event->type() == QEvent::Hide) {
            if (m_dialog) m_dialog->reject();
        } else if (watched == m_dialog && (event->type() == QEvent::Resize || event->type() == QEvent::Move)) {
            QPainterPath shape;
            shape.addRoundedRect(QRectF(m_dialog->rect()), 16, 16);
            m_dialog->setMask(QRegion(shape.toFillPolygon().toPolygon()));
            update();
        } else if (watched == m_dialog && event->type() == QEvent::LayoutRequest) {
            arrange(true);
        }
        return false;
    }
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(0, 0, 0, 135));
        if (m_dialog) {
            painter.setRenderHint(QPainter::Antialiasing);
            const auto panel = m_dialog->geometry();
            QPixmap crop(panel.size() * m_backdrop.devicePixelRatio());
            crop.setDevicePixelRatio(m_backdrop.devicePixelRatio());
            crop.fill(Qt::transparent);
            { QPainter background(&crop); background.drawPixmap(-panel.topLeft(), m_backdrop); }
            paintFrostedPanel(painter, panel, crop, 16);
        }
    }

private:
    void arrange(bool animated = false)
    {
        if (!m_dialog) return;
        auto desired = m_dialog->property("pxlPreferredSize").toSize();
        if (!desired.isValid()) desired = QSize(620, 680);
        const auto available = size() - QSize(32, 32);
        const auto bounded = desired.boundedTo(available.expandedTo(QSize(1, 1)));
        const QRect target((width() - bounded.width()) / 2, (height() - bounded.height()) / 2,
                           bounded.width(), bounded.height());
        if (animated && target == m_target && (m_dialog->geometry() == target || m_transition->state() == QAbstractAnimation::Running)) return;
        m_target = target;
        m_transition->stop();
        if (animated && isVisible() && QApplication::isEffectEnabled(Qt::UI_AnimateMenu)) {
            m_transition->setStartValue(m_dialog->geometry());
            m_transition->setEndValue(target);
            m_transition->start();
        } else m_dialog->setGeometry(target);
    }
    QPointer<QDialog> m_dialog;
    QPropertyAnimation* m_transition = nullptr;
    QRect m_target;
    QPixmap m_backdrop;
    QString m_originalStyle;
};
} // namespace Pxl

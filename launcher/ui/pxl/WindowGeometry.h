// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QSize>
#include <QWidget>

namespace Pxl {
inline QSize preferredWorkspaceWindowSize(const QSize& screen)
{
    return {qMin(1120, qMax(1, screen.width() - 64)), qMin(700, qMax(1, screen.height() - 96))};
}

inline void fitWorkspaceWindow(QWidget* window, const QRect& available)
{
    if (!window || window->isMaximized() || window->isFullScreen() || available.isEmpty()) return;
    const int frameWidth = qMax(0, window->frameGeometry().width() - window->width());
    const int frameHeight = qMax(0, window->frameGeometry().height() - window->height());
    const QSize limit(qMax(1, available.width() - frameWidth - 24),
                      qMax(1, available.height() - frameHeight - 24));
    window->setMinimumSize(window->minimumSize().boundedTo(limit));
    window->resize(window->size().boundedTo(limit));
    auto frame = window->frameGeometry();
    frame.moveLeft(qBound(available.left() + 12, frame.left(), qMax(available.left() + 12, available.right() - frame.width() - 11)));
    frame.moveTop(qBound(available.top() + 12, frame.top(), qMax(available.top() + 12, available.bottom() - frame.height() - 11)));
    window->move(frame.topLeft());
}
} // namespace Pxl

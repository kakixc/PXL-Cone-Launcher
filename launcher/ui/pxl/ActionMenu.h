// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "FrostedSurface.h"
#include <QAction>
#include <QPointer>

namespace Pxl {
// Present existing commands without changing their labels/icons in other views.
inline QAction* forwardMenuAction(QMenu* menu, QAction* command, const QString& label, const QIcon& icon = {})
{
    QIcon foreground = icon;
    if (foreground.isNull() && !command->icon().isNull()) {
        auto pixels = command->icon().pixmap(24, 24);
        QPainter painter(&pixels);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(pixels.rect(), QColor("#e8ebef"));
        painter.end();
        foreground = QIcon(pixels);
    }
    const auto shortcut = command->shortcut().toString(QKeySequence::NativeText);
    auto* action = menu->addAction(foreground, shortcut.isEmpty() ? label : label + '\t' + shortcut);
    const QPointer<QAction> source(command);
    const auto sync = [action, source] {
        action->setEnabled(source && source->isEnabled());
        if (source) {
            action->setCheckable(source->isCheckable());
            action->setChecked(source->isChecked());
        }
    };
    sync();
    QObject::connect(command, &QAction::changed, action, sync);
    QObject::connect(command, &QObject::destroyed, action, [action] { action->setEnabled(false); });
    QObject::connect(action, &QAction::triggered, command, &QAction::trigger);
    if (command->menu()) action->setMenu(command->menu());
    return action;
}
} // namespace Pxl

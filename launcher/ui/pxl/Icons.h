// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QIcon>
#include <QPainter>

namespace Pxl {
inline QIcon primaryIcon(const QString& name)
{
    auto pixmap = QIcon(":/pxl/icons/" + name + ".svg").pixmap(24, 24);
    QPainter painter(&pixmap);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(pixmap.rect(), QColor("#201a15"));
    painter.end();
    return QIcon(pixmap);
}
}

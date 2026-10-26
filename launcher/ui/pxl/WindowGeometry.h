// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QSize>

namespace Pxl {
inline QSize preferredWorkspaceWindowSize(const QSize& screen)
{
    return {qMin(1180, qMax(1, screen.width() - 48)), qMin(740, qMax(1, screen.height() - 80))};
}
} // namespace Pxl

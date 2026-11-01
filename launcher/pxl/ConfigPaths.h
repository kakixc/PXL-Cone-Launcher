// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QStringList>

namespace Pxl {
inline QStringList launcherConfigFiles(const QString& current)
{
    QStringList paths{ current, "elyprismlauncher.cfg", "prismlauncher.cfg", "polymc.cfg", "multimc.cfg" };
    paths.removeDuplicates();
    return paths;
}
}  // namespace Pxl

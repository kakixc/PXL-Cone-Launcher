// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "ConeTheme.h"

class MinecraftTheme final : public ConeTheme {
   public:
    QString id() override { return "minecraft"; }
    QString name() override;
    QString tooltip() override;
    QString appStyleSheet() override;
    QPalette colorScheme() override;
};

// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "DarkTheme.h"

class ConeTheme : public DarkTheme {
   public:
    QString id() override { return "cone"; }
    QString name() override;
    QString tooltip() override;
    QPalette colorScheme() override;
    QString appStyleSheet() override;
};

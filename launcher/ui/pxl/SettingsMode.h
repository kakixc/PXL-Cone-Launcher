// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QAbstractButton>
#include <QTabWidget>
#include <QVariant>

namespace Pxl {
// Presentation only: the original controls and their pending values stay alive.
inline void markAdvancedSetting(QWidget* widget, QAbstractButton* disclosure = nullptr)
{
    widget->setProperty("pxlAdvancedSetting", true);
    if (disclosure) widget->setProperty("pxlAdvancedDisclosure", QVariant::fromValue<QObject*>(disclosure));
}
inline void applySettingsMode(QWidget* root, bool advanced)
{
    for (auto* widget : root->findChildren<QWidget*>()) {
        if (!widget->property("pxlAdvancedSetting").toBool()) continue;
        auto* disclosure = qobject_cast<QAbstractButton*>(widget->property("pxlAdvancedDisclosure").value<QObject*>());
        widget->setVisible(advanced && (!disclosure || disclosure->isChecked()));
    }
    for (auto* tabs : root->findChildren<QTabWidget*>()) {
        for (int index = 0; index < tabs->count(); ++index)
            if (tabs->widget(index)->property("pxlAdvancedTab").toBool()) tabs->setTabVisible(index, advanced);
    }
}
} // namespace Pxl

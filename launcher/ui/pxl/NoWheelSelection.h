// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QApplication>
#include <QComboBox>
#include <QAbstractItemView>
#include <QEvent>

namespace Pxl {
class NoWheelSelection final : public QObject {
public:
    using QObject::QObject;
protected:
    bool eventFilter(QObject* object, QEvent* event) override
    {
        if (event->type() == QEvent::Wheel) {
            auto* combo = qobject_cast<QComboBox*>(object);
            if (combo && !combo->view()->isVisible()) {
                event->ignore();
                return true;
            }
        }
        return false;
    }
};
inline void installWheelSelectionGuard(QApplication* app)
{
    if (app->property("pxlWheelSelectionGuard").toBool()) return;
    app->setProperty("pxlWheelSelectionGuard", true);
    app->installEventFilter(new NoWheelSelection(app));
}
} // namespace Pxl

// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QApplication>
#include <QComboBox>
#include <QAbstractItemView>
#include <QEvent>
#include <QAbstractSpinBox>

namespace Pxl {
class NoWheelSelection final : public QObject {
public:
    using QObject::QObject;
protected:
    bool eventFilter(QObject* object, QEvent* event) override
    {
        if (event->type() == QEvent::Wheel) {
            // Native spin boxes also receive wheel events through their editor.
            auto* widget = qobject_cast<QWidget*>(object);
            if (qobject_cast<QAbstractSpinBox*>(object) ||
                (widget && qobject_cast<QAbstractSpinBox*>(widget->parentWidget()))) {
                event->ignore();
                return true;
            }
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

// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "FrostedSurface.h"
#include <QComboBox>
#include <QLineEdit>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QWheelEvent>

namespace Pxl {
inline void applySearchSurface(QLineEdit* field)
{
    field->setFixedHeight(42);
    field->setStyleSheet("QLineEdit { background: rgba(33,36,42,185); color: #f0f1f3;"
                        "border: 1px solid rgba(199,207,219,24); border-radius: 13px; padding: 0 12px; }"
                        "QLineEdit:hover { background: rgba(43,46,53,200); border-color: rgba(199,207,219,48); }"
                        "QLineEdit:focus { border-color: rgba(242,163,101,170); background: rgba(33,36,42,220); }");
}
// Opt-in PXL controls. Existing product components keep their own presentation.
class LibrarySearch final : public QLineEdit {
public:
    explicit LibrarySearch(QWidget* parent) : QLineEdit(parent)
    {
        setClearButtonEnabled(true);
        addAction(QIcon(":/pxl/icons/magnifying-glass.svg"), LeadingPosition);
        applySearchSurface(this);
    }
};

class ChoiceChip final : public QComboBox {
public:
    explicit ChoiceChip(QWidget* parent) : QComboBox(parent)
    {
        setSizeAdjustPolicy(AdjustToContents);
        setMinimumContentsLength(10);
        setFixedHeight(34);
        setStyleSheet("QComboBox { color: #e8ebef; background: rgba(43,46,53,155);"
                      "border: 1px solid rgba(199,207,219,27); border-radius: 11px; padding: 0 30px 0 12px; font-weight: 600; }"
                      "QComboBox:hover, QComboBox:focus { background: rgba(61,61,66,190); border-color: rgba(242,163,101,100); }"
                      "QComboBox::drop-down { border: none; width: 26px; }"
                      "QComboBox::down-arrow { image: url(:/pxl/icons/caret-down.svg); width: 12px; height: 12px; }");
    }
    void showPopup() override
    {
        FrostedMenu menu(this);
        menu.setMinimumWidth(width());
        const QPointer<ChoiceChip> alive(this);
        for (int row = 0; row < count(); ++row) {
            auto* action = menu.addAction(itemIcon(row), itemText(row));
            action->setCheckable(true);
            action->setChecked(row == currentIndex());
            const QPersistentModelIndex index(model()->index(row, modelColumn(), rootModelIndex()));
            action->setEnabled(index.flags().testFlag(Qt::ItemIsEnabled));
            connect(action, &QAction::triggered, this, [this, index] {
                if (index.isValid()) {
                    setCurrentIndex(index.row());
                    emit activated(index.row());
                }
            });
        }
        menu.exec(mapToGlobal(QPoint(0, height() + 6)));
        if (alive) update();
    }
protected:
    void wheelEvent(QWheelEvent* event) override { event->ignore(); }
};
} // namespace Pxl

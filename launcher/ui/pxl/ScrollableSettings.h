// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QScrollArea>
#include <QVBoxLayout>

namespace Pxl {
// Keep expanding settings sections inside the page instead of raising the window minimum size.
inline QScrollArea* makeSettingsScrollable(QVBoxLayout* pageLayout)
{
    auto* host = pageLayout->parentWidget();
    auto* content = new QWidget;
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(pageLayout->contentsMargins());
    layout->setSpacing(pageLayout->spacing());
    while (auto* item = pageLayout->takeAt(0)) {
        if (auto* widget = item->widget()) widget->setParent(content);
        layout->addItem(item);
    }
    auto* scroll = new QScrollArea(host);
    scroll->setObjectName("pxlSettingsScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
    scroll->setMinimumSize(0, 0);
    scroll->setStyleSheet("QScrollArea { background: transparent; border: none; } QScrollArea > QWidget > QWidget { background: transparent; }");
    scroll->setWidget(content);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->addWidget(scroll);
    return scroll;
}
} // namespace Pxl

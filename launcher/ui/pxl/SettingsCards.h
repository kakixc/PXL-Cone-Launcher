// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QFrame>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace Pxl {
inline QFrame* settingsCard(QWidget* parent, const QString& title, const QString& description)
{
    auto* card = new QFrame(parent);
    card->setObjectName("pxlSettingsCard");
    card->setStyleSheet("QFrame#pxlSettingsCard { background: rgba(32,35,40,235); border: 1px solid #383e47; border-radius: 12px; } QLabel { background: transparent; border: none; } QCheckBox { background: transparent; spacing: 10px; }");
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(14);
    auto* heading = new QLabel(title, card);
    heading->setStyleSheet("font-size: 16px; font-weight: 600;");
    layout->addWidget(heading);
    if (!description.isEmpty()) {
        auto* detail = new QLabel(description, card);
        detail->setWordWrap(true);
        detail->setStyleSheet("font-size: 13px; font-weight: 400; color: #a4adba;");
        layout->addWidget(detail);
    }
    return card;
}

inline QWidget* settingsRow(QWidget* parent, const QString& title, QWidget* control)
{
    auto* row = new QWidget(parent);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(20);
    auto* label = new QLabel(title, row);
    label->setWordWrap(true);
    label->setMinimumWidth(110);
    layout->addWidget(label, 1);
    layout->addWidget(control, 2);
    return row;
}
}

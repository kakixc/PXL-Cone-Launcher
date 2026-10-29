// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "DialogOverlay.h"
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <optional>

namespace Pxl {
// Uses the same PXL flow surface as instance renaming, with caller-owned copy.
inline std::optional<QString> nameEntryPanel(QWidget* surface, const QString& title, const QString& label,
                                             const QString& current, const QString& action)
{
    QDialog panel(surface);
    panel.setObjectName("pxlNameEntry");
    panel.setProperty("pxlFlow", true);
    panel.setProperty("pxlPreferredSize", QSize(520, 250));
    auto* layout = new QVBoxLayout(&panel);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);
    auto* heading = new QLabel(title, &panel);
    heading->setTextFormat(Qt::PlainText);
    heading->setWordWrap(true);
    heading->setProperty("pxlFlowTitle", true);
    layout->addWidget(heading);
    auto* caption = new QLabel(label, &panel);
    caption->setTextFormat(Qt::PlainText);
    caption->setWordWrap(true);
    layout->addWidget(caption);
    auto* name = new QLineEdit(current, &panel);
    name->setObjectName("pxlEnteredName");
    name->setAccessibleName(label);
    name->setMinimumHeight(42);
    caption->setBuddy(name);
    layout->addWidget(name);
    layout->addStretch();
    auto* actions = new QHBoxLayout;
    actions->addStretch();
    auto* cancel = new QPushButton(QCoreApplication::translate("Pxl::MessagePanel", "Cancel"), &panel);
    auto* save = new QPushButton(action, &panel);
    save->setObjectName("pxlNameSave");
    save->setProperty("pxlPrimary", true);
    save->setDefault(true);
    save->setEnabled(!current.trimmed().isEmpty());
    actions->addWidget(cancel);
    actions->addWidget(save);
    layout->addLayout(actions);
    QObject::connect(cancel, &QPushButton::clicked, &panel, &QDialog::reject);
    QObject::connect(save, &QPushButton::clicked, &panel, &QDialog::accept);
    QObject::connect(name, &QLineEdit::textChanged, save, [save](const QString& text) {
        save->setEnabled(!text.trimmed().isEmpty());
    });
    QTimer::singleShot(0, name, [name] { name->setFocus(); name->selectAll(); });
    if (DialogOverlay::run(&panel, surface->window()) != QDialog::Accepted || name->text().trimmed().isEmpty())
        return std::nullopt;
    return name->text();
}
} // namespace Pxl

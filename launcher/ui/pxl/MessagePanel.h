// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QCoreApplication>
#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>
#include "DialogOverlay.h"

namespace Pxl {
inline bool messagePanel(QWidget* surface, const QString& title, const QString& message,
                         const QString& action, bool confirmation = false)
{
    QDialog panel(surface);
    panel.setObjectName("pxlMessagePanel");
    panel.setProperty("pxlFlow", true);
    panel.setProperty("pxlPreferredSize", QSize(560, message.size() > 350 ? 420 : 280));
    auto* layout = new QVBoxLayout(&panel);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(20);
    auto* heading = new QLabel(title, &panel);
    heading->setWordWrap(true);
    heading->setTextFormat(Qt::PlainText);
    heading->setProperty("pxlFlowTitle", true);
    layout->addWidget(heading);
    auto* body = new QTextEdit(&panel);
    body->setReadOnly(true);
    body->setPlainText(message);
    body->setFrameShape(QFrame::NoFrame);
    body->setStyleSheet("QTextEdit { background: transparent; border: none; padding: 0; }");
    layout->addWidget(body, 1);
    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    if (confirmation) {
        auto* cancel = new QPushButton(QCoreApplication::translate("Pxl::MessagePanel", "Cancel"), &panel);
        cancel->setDefault(true);
        buttons->addWidget(cancel);
        QObject::connect(cancel, &QPushButton::clicked, &panel, &QDialog::reject);
    }
    auto* done = new QPushButton(action, &panel);
    done->setProperty("pxlPrimary", true);
    done->setAutoDefault(!confirmation);
    buttons->addWidget(done);
    QObject::connect(done, &QPushButton::clicked, &panel, &QDialog::accept);
    layout->addLayout(buttons);
    return DialogOverlay::run(&panel, surface->window()) == QDialog::Accepted;
}
inline void showNotice(QWidget* surface, const QString& title, const QString& message)
{
    messagePanel(surface, title, message, QCoreApplication::translate("Pxl::MessagePanel", "Done"));
}
} // namespace Pxl

// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QCoreApplication>
#include <QLabel>
#include <QTextEdit>
#include <QVBoxLayout>

namespace Pxl {
inline QWidget* loginErrorPanel(QWidget* parent, const QString& reason)
{
    auto* panel = new QWidget(parent);
    panel->setObjectName("pxlLoginError");
    auto* layout = new QVBoxLayout(panel); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(14);
    auto* title = new QLabel(QCoreApplication::translate("MSALoginDialog", "Could not sign in"), panel);
    title->setProperty("pxlFlowTitle", true); layout->addWidget(title);
    auto* hint = new QLabel(QCoreApplication::translate("MSALoginDialog", "The account was not added. You can try again or add it later."), panel);
    hint->setWordWrap(true); hint->setProperty("pxlFlowHint", true); layout->addWidget(hint);
    auto* detail = new QTextEdit(panel); detail->setObjectName("loginFailureReason");
    detail->setReadOnly(true); detail->setPlainText(reason);
    detail->setStyleSheet("QTextEdit { background:rgba(255,255,255,8); color:#e4bb9b; border:1px solid rgba(255,255,255,20); border-radius:10px; padding:10px; }");
    layout->addWidget(detail, 1);
    return panel;
}
}

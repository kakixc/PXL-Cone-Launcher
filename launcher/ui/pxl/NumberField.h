// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QCoreApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QSpinBox>
#include <QToolButton>

namespace Pxl {
// Retain the original spin box's value, validation, suffix and keyboard editing.
class NumberField final : public QFrame {
public:
    explicit NumberField(QSpinBox* source, QWidget* parent = nullptr) : QFrame(parent)
    {
        setObjectName("pxlNumberField");
        setFixedWidth(184);
        setFixedHeight(40);
        setStyleSheet("QFrame#pxlNumberField { background:rgba(20,24,29,140); border:1px solid #3b424b; border-radius:10px; }"
                      "QSpinBox { background:transparent; color:#edf0f4; border:none; padding:0 3px; min-height:32px; }"
                      "QToolButton { background:transparent; color:#cbd1d9; border:none; border-radius:7px; font-size:20px; font-weight:600; }"
                      "QToolButton:hover { background:rgba(242,163,101,35); color:#f2b17d; }"
                      "QToolButton:pressed { background:rgba(242,163,101,65); }");
        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(4,4,4,4);
        layout->setSpacing(2);
        auto* minus = new QToolButton(this);
        minus->setObjectName("pxlNumberDecrease"); minus->setText(QString::fromUtf8("−"));
        minus->setAccessibleName(QCoreApplication::translate("Pxl::NumberField","Decrease"));
        minus->setFixedSize(28,30);
        layout->addWidget(minus);
        source->setParent(this);
        source->setButtonSymbols(QAbstractSpinBox::NoButtons);
        source->setMinimumWidth(0);
        source->setAlignment(Qt::AlignCenter);
        source->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
        layout->addWidget(source,1);
        auto* plus = new QToolButton(this);
        plus->setObjectName("pxlNumberIncrease"); plus->setText("+");
        plus->setAccessibleName(QCoreApplication::translate("Pxl::NumberField","Increase"));
        plus->setFixedSize(28,30);
        layout->addWidget(plus);
        setFocusProxy(source);
        connect(minus,&QToolButton::clicked,source,&QAbstractSpinBox::stepDown);
        connect(plus,&QToolButton::clicked,source,&QAbstractSpinBox::stepUp);
    }
};
}

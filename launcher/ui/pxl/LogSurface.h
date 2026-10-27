// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "LibraryControls.h"
#include "ToggleSwitch.h"
#include "ui/widgets/LogView.h"
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace Pxl {
inline QString logText(const char* text) { return QCoreApplication::translate("Pxl::LogSurface", text); }
class LogSurface final : public QWidget {
public:
    LogSurface(QAbstractItemModel* model, bool tracking, bool wrapping, bool coloring, QWidget* parent)
        : QWidget(parent)
    {
        setObjectName("pxlLogSurface");
        auto* layout = new QVBoxLayout(this); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(14);
        auto* options = new QHBoxLayout; options->setSpacing(14);
        follow = new ToggleSwitch(logText("Follow output"), this); follow->setObjectName("logFollow"); follow->setChecked(tracking);
        wrap = new ToggleSwitch(logText("Wrap lines"), this); wrap->setObjectName("logWrap"); wrap->setChecked(wrapping);
        colors = new ToggleSwitch(logText("Color levels"), this); colors->setObjectName("logColors"); colors->setChecked(coloring);
        options->addWidget(follow); options->addWidget(wrap); options->addWidget(colors); options->addStretch(); layout->addLayout(options);
        auto* stack = new QStackedWidget(this); layout->addWidget(stack, 1);
        view = new LogView(stack); view->setObjectName("pxlLogView"); view->setReadOnly(true);
        view->setFrameShape(QFrame::NoFrame); view->setWordWrap(wrapping); view->setColorLines(coloring);
        view->setStyleSheet("QPlainTextEdit { background: rgba(24,28,33,210); border: 1px solid rgba(190,205,225,25); border-radius: 12px; padding: 12px; }");
        view->setModel(model); stack->addWidget(view);
        auto* empty = new QWidget(stack); auto* emptyLayout = new QVBoxLayout(empty); emptyLayout->setSpacing(14); emptyLayout->addStretch();
        auto* icon = new QLabel(empty); icon->setAlignment(Qt::AlignCenter); icon->setPixmap(QIcon(":/pxl/icons/sliders-horizontal.svg").pixmap(44, 44)); emptyLayout->addWidget(icon);
        auto* title = new QLabel(logText("No log output yet"), empty); title->setObjectName("logEmptyTitle"); title->setAlignment(Qt::AlignCenter); title->setStyleSheet("font-size: 20px; font-weight: 600;"); emptyLayout->addWidget(title);
        auto* hint = new QLabel(logText("Launch this instance to see its output and error details here."), empty); hint->setWordWrap(true); hint->setAlignment(Qt::AlignCenter); hint->setProperty("pxlFlowHint", true); emptyLayout->addWidget(hint);
        emptyLayout->addStretch(); stack->addWidget(empty);
        auto* tools = new QHBoxLayout; tools->setSpacing(10);
        search = new LibrarySearch(this); search->setObjectName("logSearch"); search->setPlaceholderText(logText("Find in log")); tools->addWidget(search, 1);
        findButton = new QPushButton(logText("Find next"), this); tools->addWidget(findButton);
        bottomButton = new QPushButton(logText("Latest output"), this); tools->addWidget(bottomButton);
        layout->addLayout(tools);
        auto* actions = new QHBoxLayout; actions->setSpacing(10);
        auto* privacy = new QLabel(logText("Review the log before sharing it."), this); privacy->setWordWrap(true); privacy->setProperty("pxlFlowHint", true); actions->addWidget(privacy, 1);
        copyButton = new QPushButton(logText("Copy log"), this); actions->addWidget(copyButton);
        shareButton = new QPushButton(logText("Share log"), this); actions->addWidget(shareButton);
        clearButton = new QPushButton(logText("Clear"), this); actions->addWidget(clearButton); layout->addLayout(actions);
        const auto refresh = [=, this] {
            const bool any = model->rowCount() > 0;
            stack->setCurrentIndex(any ? 0 : 1);
            for (auto* button : {copyButton, shareButton, clearButton, findButton, bottomButton}) button->setEnabled(any);
            search->setEnabled(any);
        };
        connect(model, &QAbstractItemModel::rowsInserted, this, refresh); connect(model, &QAbstractItemModel::rowsRemoved, this, refresh);
        connect(model, &QAbstractItemModel::modelReset, this, refresh); refresh();
    }
    LogView* view;
    LibrarySearch* search;
    ToggleSwitch* follow;
    ToggleSwitch* wrap;
    ToggleSwitch* colors;
    QPushButton* copyButton;
    QPushButton* shareButton;
    QPushButton* clearButton;
    QPushButton* findButton;
    QPushButton* bottomButton;
};
} // namespace Pxl

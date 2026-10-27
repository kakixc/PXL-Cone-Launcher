// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include "TaskHistory.h"
#include <algorithm>

namespace Pxl {
// A single quiet activity strip across pages, backed by the same task history.
class TaskActivity final : public QWidget {
public:
    explicit TaskActivity(TaskHistory* history, QWidget* parent = nullptr) : QWidget(parent)
    {
        setObjectName("pxlTaskActivity");
        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(28, 8, 24, 8);
        layout->setSpacing(12);
        auto* button = new QPushButton(this);
        button->setObjectName("taskActivityOpen");
        button->setIcon(QIcon(":/pxl/icons/download-simple.svg"));
        button->setCursor(Qt::PointingHandCursor);
        layout->addWidget(button);
        auto* title = new QLabel(this);
        title->setTextFormat(Qt::PlainText);
        title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        title->setMinimumWidth(0);
        layout->addWidget(title, 1);
        auto* progress = new QProgressBar(this);
        progress->setObjectName("taskActivityProgress");
        progress->setTextVisible(false);
        progress->setFixedHeight(5);
        progress->setMaximumWidth(300);
        layout->addWidget(progress, 1);
        connect(button, &QPushButton::clicked, history, [history, button] { history->showTasks(button); });
        auto refresh = [this, history, button, title, progress] {
            int count = 0;
            double fraction = 0;
            bool known = true;
            QString latest;
            for (const auto& entry : history->records()) {
                if (!entry.active()) continue;
                ++count;
                latest = entry.title;
                known &= entry.total > 0;
                if (entry.total > 0) fraction += std::clamp(double(entry.current) / entry.total, 0.0, 1.0);
            }
            setVisible(count > 0);
            button->setText(QCoreApplication::translate("Pxl::TaskActivity", "Downloads · %1").arg(count));
            title->setText(latest);
            title->setToolTip(latest);
            progress->setRange(0, known ? 1000 : 0);
            progress->setValue(count > 0 && known ? int(1000 * fraction / count) : 0);
        };
        auto* throttle = new QTimer(this);
        throttle->setInterval(100);
        throttle->setSingleShot(true);
        connect(throttle, &QTimer::timeout, this, refresh);
        connect(history, &TaskHistory::changed, this, [throttle] { if (!throttle->isActive()) throttle->start(); });
        refresh();
    }
};
} // namespace Pxl

/// SPDX-License-Identifier: GPL-3.0-only
/*
 *  PrismLaucher - Minecraft Launcher
 *  Copyright (C) 2023 Rachel Powers <508861+Ryex@users.noreply.github.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2013-2021 MultiMC Contributors
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#include "ProgressDialog.h"
#include <QPoint>
#include "ui_ProgressDialog.h"

#include <QDebug>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QEventLoop>
#include <algorithm>
#include <limits>

#include "tasks/Task.h"

#include "ui/widgets/SubTaskProgressBar.h"
#include "ui/pxl/TaskHistory.h"

// map a value in a numeric range of an arbitrary type to between 0 and INT_MAX
// for getting the best precision out of the qt progress bar
template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, bool> = true>
std::tuple<int, int> map_int_zero_max(T current, T range_max, T range_min)
{
    int int_max = std::numeric_limits<int>::max();

    auto type_range = range_max - range_min;
    if (type_range <= 0) return {0, 0};
    double percentage = std::clamp(static_cast<double>(current - range_min) / static_cast<double>(type_range), 0.0, 1.0);
    int mapped_current = percentage * int_max;

    return { mapped_current, int_max };
}

ProgressDialog::ProgressDialog(QWidget* parent) : QDialog(parent), ui(new Ui::ProgressDialog)
{
    ui->setupUi(this);
    setProperty("pxlFlow", true);
    setWindowTitle(tr("Downloads & tasks"));
    setWindowIcon(QIcon(":/pxl/cone-icon.svg"));
    ui->verticalLayout->setContentsMargins(28, 24, 28, 24);
    ui->verticalLayout->setSpacing(20);
    auto* header = new QHBoxLayout;
    auto* brand = new QLabel(this);
    brand->setPixmap(QIcon(":/pxl/cone-icon.svg").pixmap(36, 36));
    header->addWidget(brand);
    auto* title = new QLabel(tr("Downloads & tasks"), this);
    title->setProperty("pxlFlowTitle", true);
    header->addWidget(title, 1);
    auto* tasksButton = new QPushButton(this);
    tasksButton->setIcon(QIcon(":/pxl/icons/download-simple.svg"));
    tasksButton->setToolTip(tr("View all tasks"));
    tasksButton->setAccessibleName(tr("View all tasks"));
    tasksButton->setFixedSize(40, 36);
    header->addWidget(tasksButton);
    connect(tasksButton, &QPushButton::clicked, this, [this] { Pxl::TaskHistory::instance()->showTasks(this); });
    m_percentage = new QLabel(this);
    m_percentage->setStyleSheet("font-size: 20px; font-weight: 700;");
    header->addWidget(m_percentage);
    ui->verticalLayout->insertLayout(0, header);
    ui->globalStatusLabel->clear();
    ui->globalStatusDetailsLabel->clear();
    ui->globalStatusDetailsLabel->setWordWrap(true);
    ui->globalStatusDetailsLabel->setProperty("pxlFlowHint", true);
    ui->globalProgressBar->setTextVisible(false);
    ui->globalProgressBar->setFixedHeight(10);
    ui->taskProgressScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->taskProgressLayout->setContentsMargins(0, 0, 0, 0);
    ui->taskProgressLayout->setSpacing(12);
    m_detailsButton = new QPushButton(tr("Show details"), this);
    m_detailsButton->setProperty("pxlQuiet", true);
    m_detailsButton->setCheckable(true);
    m_detailsButton->setVisible(false);
    ui->verticalLayout->insertWidget(3, m_detailsButton, 0, Qt::AlignLeft);
    connect(m_detailsButton, &QPushButton::toggled, this, [this](bool show) {
        m_showDetails = show;
        m_detailsButton->setText(show ? tr("Hide details") : tr("Show details"));
        ui->taskProgressScrollArea->setVisible(show && m_is_multi_step);
        updateSize();
    });
    ui->taskProgressScrollArea->setHidden(true);
    this->setWindowFlags(this->windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setAttribute(Qt::WidgetAttribute::WA_QuitOnClose, true);
    changeProgress(0, 100);
    updateSize(true);
    setSkipButton(false);
}

void ProgressDialog::setSkipButton(bool present, QString label)
{
    ui->skipButton->setAutoDefault(false);
    ui->skipButton->setDefault(false);
    ui->skipButton->setFocusPolicy(Qt::ClickFocus);
    ui->skipButton->setEnabled(present);
    ui->skipButton->setVisible(present);
    ui->skipButton->setText(label);
    updateSize();
}

void ProgressDialog::setBackgroundable()
{
    if (m_backgroundable) return;
    m_backgroundable = true;
    const auto center = parentWidget() ? parentWidget()->mapToGlobal(parentWidget()->rect().center()) : geometry().center();
    // This dialog is stack-owned by the launch controller. Navigating away
    // must not delete it through an embedded editor's QObject ownership.
    setParent(nullptr, Qt::Dialog);
    setAttribute(Qt::WA_QuitOnClose, false);
    move(center - QPoint(width() / 2, height() / 2));
    ui->verticalLayout->removeWidget(ui->skipButton);
    auto* footer = new QHBoxLayout;
    footer->addStretch();
    auto* hideButton = new QPushButton(tr("Hide"), this);
    hideButton->setObjectName("progressHide");
    hideButton->setAutoDefault(false);
    hideButton->setToolTip(tr("Continue in Downloads & tasks"));
    connect(hideButton, &QPushButton::clicked, this, &QWidget::hide);
    footer->addWidget(hideButton);
    footer->addWidget(ui->skipButton);
    ui->verticalLayout->addLayout(footer);
}

void ProgressDialog::on_skipButton_clicked(bool checked)
{
    Q_UNUSED(checked);
    if (m_task && ui->skipButton->isEnabled())  // prevent other triggers from aborting
        m_task->abort();
}

ProgressDialog::~ProgressDialog()
{
    for (auto conn : this->m_taskConnections) {
        disconnect(conn);
    }
    delete ui;
}

void ProgressDialog::updateSize(bool recenterParent)
{
    QSize lastSize = this->size();
    QPoint lastPos = this->pos();
    int minHeight = ui->globalStatusDetailsLabel->minimumSize().height() + (ui->verticalLayout->spacing() * 2);
    minHeight += ui->globalProgressBar->minimumSize().height() + ui->verticalLayout->spacing();
    if (!ui->taskProgressScrollArea->isHidden())
        minHeight += ui->taskProgressScrollArea->minimumSizeHint().height() + ui->verticalLayout->spacing();
    if (ui->skipButton->isVisible())
        minHeight += ui->skipButton->height() + ui->verticalLayout->spacing();
    minHeight = std::max(minHeight, 60);
    QSize minSize = QSize(560, qMax(minHeight, m_showDetails ? 360 : 220));

    setMinimumSize(minSize);
    adjustSize();

    QSize newSize = this->size();
    // if the current window is a different size
    auto parent = this->parentWidget();
    if (recenterParent && parent) {
        auto newX = std::max(0, parent->x() + ((parent->width() - newSize.width()) / 2));
        auto newY = std::max(0, parent->y() + ((parent->height() - newSize.height()) / 2));
        this->move(newX, newY);
    } else if (lastSize != newSize) {
        // center on old position after resize
        QSize sizeDiff = lastSize - newSize;  // last size was smaller, the results should be negative
        auto newX = std::max(0, lastPos.x() + (sizeDiff.width() / 2));
        auto newY = std::max(0, lastPos.y() + (sizeDiff.height() / 2));
        this->move(newX, newY);
    }
}

int ProgressDialog::execWithTask(Task* task)
{
    this->m_task = task;

    if (!task) {
        qDebug() << "Programmer error: Progress dialog created with null task.";
        return QDialog::DialogCode::Accepted;
    }
    Pxl::TaskHistory::instance()->watch(task, windowTitle(), windowIcon());

    QDialog::DialogCode result{};
    if (handleImmediateResult(result)) {
        return result;
    }

    // Connect signals.
    this->m_taskConnections.push_back(connect(task, &Task::started, this, &ProgressDialog::onTaskStarted));
    this->m_taskConnections.push_back(connect(task, &Task::failed, this, &ProgressDialog::onTaskFailed));
    this->m_taskConnections.push_back(connect(task, &Task::succeeded, this, &ProgressDialog::onTaskSucceeded));
    this->m_taskConnections.push_back(connect(task, &Task::status, this, &ProgressDialog::changeStatus));
    this->m_taskConnections.push_back(connect(task, &Task::details, this, &ProgressDialog::changeStatus));
    this->m_taskConnections.push_back(connect(task, &Task::stepProgress, this, &ProgressDialog::changeStepProgress));
    this->m_taskConnections.push_back(connect(task, &Task::progress, this, &ProgressDialog::changeProgress));
    this->m_taskConnections.push_back(connect(task, &Task::aborted, this, [this] {
        if (m_backgroundable) reject();
        else hide();
    }));
    this->m_taskConnections.push_back(connect(task, &Task::abortStatusChanged, ui->skipButton, &QPushButton::setEnabled));
    this->m_taskConnections.push_back(connect(task, &Task::abortButtonTextChanged, ui->skipButton, &QPushButton::setText));

    m_is_multi_step = task->isMultiStep();
    m_detailsButton->setVisible(m_is_multi_step);
    ui->taskProgressScrollArea->setVisible(m_is_multi_step && m_showDetails);
    updateSize();

    // It's a good idea to start the task after we entered the dialog's event loop :^)
    if (!task->isRunning()) {
        QMetaObject::invokeMethod(task, &Task::start, Qt::QueuedConnection);
    } else {
        changeStatus(task->getStatus());
        changeProgress(task->getProgress(), task->getTotalProgress());
    }

    if (!m_backgroundable) return QDialog::exec();

    // QDialog::exec() returns when hidden. Keep the launch stack waiting for
    // the actual task result instead; Hide only changes presentation.
    QEventLoop wait;
    connect(this, &QDialog::finished, &wait, &QEventLoop::quit);
    const auto destroyed = connect(task, &QObject::destroyed, this, &QDialog::reject);
    setWindowModality(Qt::ApplicationModal);
    show();
    if (!task->isFinished()) wait.exec();
    disconnect(destroyed);
    hide();
    return this->result();
}

// TODO: only provide the unique_ptr overloads
int ProgressDialog::execWithTask(std::unique_ptr<Task>&& task)
{
    connect(this, &ProgressDialog::destroyed, task.get(), &Task::deleteLater);
    return execWithTask(task.release());
}
int ProgressDialog::execWithTask(std::unique_ptr<Task>& task)
{
    connect(this, &ProgressDialog::destroyed, task.get(), &Task::deleteLater);
    return execWithTask(task.release());
}

bool ProgressDialog::handleImmediateResult(QDialog::DialogCode& result)
{
    if (m_task->isFinished()) {
        if (m_task->wasSuccessful()) {
            result = QDialog::Accepted;
        } else {
            result = QDialog::Rejected;
        }
        return true;
    }
    return false;
}

Task* ProgressDialog::getTask()
{
    return m_task;
}

void ProgressDialog::onTaskStarted() {}

void ProgressDialog::onTaskFailed([[maybe_unused]] QString failure)
{
    reject();
    hide();
}

void ProgressDialog::onTaskSucceeded()
{
    accept();
    hide();
}

void ProgressDialog::changeStatus([[maybe_unused]] const QString& status)
{
    if (!m_task) return;
    ui->globalStatusLabel->setText(m_task->getStatus());
    ui->globalStatusLabel->adjustSize();
    ui->globalStatusDetailsLabel->setText(m_task->getDetails());
    ui->globalStatusDetailsLabel->adjustSize();

    updateSize();
}

void ProgressDialog::addTaskProgress(TaskStepProgress const& progress)
{
    SubTaskProgressBar* task_bar = new SubTaskProgressBar(this);
    taskProgress.insert(progress.uid, task_bar);
    ui->taskProgressLayout->addWidget(task_bar);
}

void ProgressDialog::changeStepProgress(TaskStepProgress const& task_progress)
{
    m_is_multi_step = true;
    m_detailsButton->setVisible(true);
    if (m_showDetails && ui->taskProgressScrollArea->isHidden()) {
        ui->taskProgressScrollArea->setVisible(true);
        updateSize();
    }

    if (!taskProgress.contains(task_progress.uid))
        addTaskProgress(task_progress);
    auto task_bar = taskProgress.value(task_progress.uid);

    auto const [mapped_current, mapped_total] = map_int_zero_max<qint64>(task_progress.current, task_progress.total, 0);
    if (task_progress.total <= 0) {
        task_bar->setRange(0, 0);
    } else {
        task_bar->setRange(0, mapped_total);
    }

    task_bar->setValue(mapped_current);
    task_bar->setStatus(task_progress.status);
    task_bar->setDetails(task_progress.details);
    task_bar->setVisible(!task_progress.isDone());
}

void ProgressDialog::changeProgress(qint64 current, qint64 total)
{
    const auto [value, maximum] = map_int_zero_max<qint64>(qBound<qint64>(0, current, qMax<qint64>(0, total)), total, 0);
    ui->globalProgressBar->setRange(0, maximum);
    ui->globalProgressBar->setValue(value);
    if (m_percentage)
        m_percentage->setText(total > 0 ? QString::number(int(std::clamp(100.0 * current / total, 0.0, 100.0))) + "%" : tr("Working…"));
}

void ProgressDialog::keyPressEvent(QKeyEvent* e)
{
    if (m_backgroundable && e->key() == Qt::Key_Escape && m_task && m_task->isRunning()) {
        hide();
        e->accept();
        return;
    }
    if (ui->skipButton->isVisible()) {
        if (e->key() == Qt::Key_Escape) {
            on_skipButton_clicked(true);
            return;
        } else if (e->key() == Qt::Key_Tab) {
            ui->skipButton->setFocusPolicy(Qt::StrongFocus);
            ui->skipButton->setFocus();
            ui->skipButton->setAutoDefault(true);
            ui->skipButton->setDefault(true);
            return;
        }
    }
    QDialog::keyPressEvent(e);
}

void ProgressDialog::closeEvent(QCloseEvent* e)
{
    if (m_task && m_task->isRunning()) {
        e->ignore();
        if (m_backgroundable) hide();
    } else {
        QDialog::closeEvent(e);
    }
}

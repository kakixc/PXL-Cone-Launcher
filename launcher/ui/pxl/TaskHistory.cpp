// SPDX-License-Identifier: GPL-3.0-only
#include "TaskHistory.h"

#include <QApplication>
#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QScreen>
#include <QToolTip>
#include <QPainter>
#include <QPainterPath>
#include "Animations.h"
#include "FrostedSurface.h"
#include <algorithm>

namespace Pxl {
class TaskPopover final : public QDialog {
public:
    explicit TaskPopover(QWidget* parent) : QDialog(parent, Qt::Popup | Qt::FramelessWindowHint) {}
    void updateBackdrop()
    {
        const QRect area(mapToGlobal(QPoint()), size());
        if (area == m_captureArea && !m_backdrop.isNull()) return;
        m_captureArea = area;
        m_backdrop = frostedBackdrop(parentWidget(), area);
    }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        paintFrostedPanel(painter, rect(), m_backdrop);
    }
private:
    QPixmap m_backdrop;
    QRect m_captureArea;
};
TaskHistory::TaskHistory(QObject* parent) : QObject(parent) {}

TaskHistory* TaskHistory::instance()
{
    static QPointer<TaskHistory> history;
    if (!history) history = new TaskHistory(qApp);
    return history;
}

TaskHistory::Record* TaskHistory::record(quint64 id)
{
    for (auto& item : m_records) if (item.id == id) return &item;
    return nullptr;
}

int TaskHistory::activeCount() const
{
    return int(std::count_if(m_records.cbegin(), m_records.cend(), [](const auto& item) { return item.active(); }));
}

void TaskHistory::trimHistory()
{
    // Retain every active operation. Only the completed history is capped;
    // dropping a live task would hide its progress and cancellation control.
    auto completed = std::count_if(m_records.cbegin(), m_records.cend(), [](const auto& item) { return !item.active(); });
    while (completed > 50) {
        const auto first = std::find_if(m_records.begin(), m_records.end(), [](const auto& item) { return !item.active(); });
        if (first == m_records.end()) break;
        m_records.erase(first);
        --completed;
    }
}

void TaskHistory::watch(Task* task, const QString& title, const QIcon& icon)
{
    if (!task) return;
    for (const auto& item : m_records) if (item.task == task) return;
    Record item;
    item.id = ++m_nextId;
    item.title = task->property("pxlTaskTitle").toString();
    if (item.title.isEmpty()) item.title = title.isEmpty() ? tr("Task") : title;
    item.icon = qvariant_cast<QIcon>(task->property("pxlTaskIcon"));
    if (item.icon.isNull()) item.icon = icon;
    item.task = task;
    item.state = task->getState();
    item.status = task->getStatus();
    item.details = task->getDetails();
    item.current = task->getProgress();
    item.total = task->getTotalProgress();
    item.error = task->failReason();
    item.canAbort = task->canAbort();
    if (!item.active()) item.finishedAt = QDateTime::currentDateTime();
    const auto id = item.id;
    m_records.append(item);
    connect(task, &Task::started, this, [this, id] {
        if (auto* entry = record(id); entry && entry->active()) entry->state = Task::State::Running;
        notifyChanged();
    });
    connect(task, &Task::progress, this, [this, id](qint64 current, qint64 total) {
        if (auto* entry = record(id)) { entry->current = current; entry->total = total; }
        notifyChanged();
    });
    connect(task, &Task::status, this, [this, id](const QString& status) {
        if (auto* entry = record(id)) entry->status = status;
        notifyChanged();
    });
    connect(task, &Task::details, this, [this, id](const QString& details) {
        if (auto* entry = record(id)) entry->details = details;
        notifyChanged();
    });
    connect(task, &Task::abortStatusChanged, this, [this, id](bool canAbort) {
        if (auto* entry = record(id)) entry->canAbort = canAbort;
        notifyChanged();
    });
    auto finish = [this, id, task](Task::State state, const QString& error = {}) {
        if (auto* entry = record(id)) {
            entry->state = state;
            entry->error = error;
            if (!task->warnings().isEmpty()) entry->details += '\n' + task->warnings().join('\n');
            entry->finishedAt = QDateTime::currentDateTime();
        }
        notifyChanged();
    };
    connect(task, &Task::succeeded, this, [finish] { finish(Task::State::Succeeded); });
    connect(task, &Task::failed, this, [finish](const QString& error) { finish(Task::State::Failed, error); });
    connect(task, &Task::aborted, this, [finish] { finish(Task::State::AbortedByUser); });
    connect(task, &QObject::destroyed, this, [this, id] {
        if (auto* entry = record(id); entry && entry->active()) {
            entry->state = Task::State::Failed;
            entry->error = tr("Task interrupted");
            entry->finishedAt = QDateTime::currentDateTime();
        }
        notifyChanged();
    });
    // Completion may have happened between the initial snapshot and connecting
    // its terminal signals. Reconcile once after all observers are installed.
    if (task->isFinished()) finish(task->getState(), task->failReason());
    notifyChanged();
}

void TaskHistory::startOwned(std::unique_ptr<Task> task, const QString& title, const QIcon& icon)
{
    if (!task) return;
    auto* operation = task.release();
    operation->setParent(this);
    watch(operation, title, icon);
    connect(operation, &Task::finished, operation, &QObject::deleteLater);
    if (operation->isFinished()) operation->deleteLater();
    else if (!operation->isRunning()) QTimer::singleShot(0, operation, &Task::start);
}

void TaskHistory::notifyChanged()
{
    trimHistory();
    emit changed();
}

bool TaskHistory::cancel(quint64 id)
{
    auto* entry = record(id);
    return entry && entry->state == Task::State::Running && entry->task && entry->canAbort && entry->task->abort();
}

void TaskHistory::clearFinished()
{
    m_records.removeIf([](const auto& item) { return !item.active(); });
    emit changed();
}

void TaskHistory::showTasks(QWidget* parent)
{
    if (m_dialog) {
        m_dialog->show();
        m_dialog->raise();
        m_dialog->activateWindow();
        return;
    }
    auto* dialog = new TaskPopover(parent);
    m_dialog = dialog;
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setAttribute(Qt::WA_TranslucentBackground);
    dialog->setObjectName("pxlTaskPopover");
    dialog->setStyleSheet("QDialog#pxlTaskPopover { background: transparent; border: none; }");
    dialog->setProperty("pxlFlow", true);
    dialog->setWindowTitle(tr("Downloads & tasks"));
    dialog->setWindowIcon(QIcon(":/pxl/cone-icon.svg"));
    dialog->resize(400, 160);
    auto* layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(8);
    auto* header = new QHBoxLayout;
    auto* heading = new QLabel(tr("Downloads & tasks"), dialog);
    heading->setProperty("pxlFlowTitle", true);
    heading->setStyleSheet("font-size: 16px; font-weight: 700;");
    header->addWidget(heading, 1);
    auto* clear = new QPushButton(tr("Clear completed"), dialog);
    clear->setProperty("pxlQuiet", true);
    clear->setStyleSheet("font-size: 12px; padding: 6px;");
    header->addWidget(clear);
    layout->addLayout(header);
    auto* list = new QListWidget(dialog);
    list->setObjectName("taskHistoryList");
    list->setSelectionMode(QAbstractItemView::NoSelection);
    list->setSpacing(6);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setStyleSheet("QListWidget { background: transparent; border: none; }");
    layout->addWidget(list, 1);
    auto* empty = new QLabel(tr("No tasks yet. Downloads and installations will appear here."), dialog);
    empty->setWordWrap(true);
    empty->setProperty("pxlFlowHint", true);
    layout->addWidget(empty);
    auto refresh = [this, list, clear, empty, dialog, parent] {
        const int position = list->verticalScrollBar()->value();
        list->clear();
        bool completed = false;
        for (auto it = m_records.crbegin(); it != m_records.crend(); ++it) {
            const auto& entry = *it;
            completed |= !entry.active();
            auto* row = new QWidget(list);
            auto* body = new QVBoxLayout(row);
            body->setContentsMargins(6, 8, 6, 8);
            body->setSpacing(6);
            auto* line = new QHBoxLayout;
            auto* icon = new QLabel(row);
            icon->setPixmap((entry.icon.isNull() ? QIcon(":/pxl/icons/download-simple.svg") : entry.icon).pixmap(28, 28));
            line->addWidget(icon);
            auto* title = new QLabel(entry.title, row);
            title->setTextFormat(Qt::PlainText);
            title->setWordWrap(true);
            title->setStyleSheet("font-weight: 700;");
            line->addWidget(title, 1);
            auto* details = new QPushButton(tr("Details"), row);
            details->setStyleSheet("font-size: 12px; padding: 5px;");
            details->setProperty("pxlQuiet", true);
            line->addWidget(details);
            connect(details, &QPushButton::clicked, row, [this, id = entry.id, details, dialog] {
                if (const auto* item = record(id))
                    QToolTip::showText(details->mapToGlobal(QPoint(0, details->height())),
                                      "<div>" + QStringList{item->status, item->details, item->error}.join('\n').toHtmlEscaped().replace("\n", "<br>") + "</div>", dialog);
            });
            if (entry.state == Task::State::Running && entry.task && entry.canAbort) {
                auto* cancel = new QPushButton(tr("Cancel"), row);
                line->addWidget(cancel);
                connect(cancel, &QPushButton::clicked, row, [this, id = entry.id] { this->cancel(id); });
            }
            body->addLayout(line);
            QString status = entry.status;
            if (!entry.active()) {
                status = entry.state == Task::State::Succeeded ? tr("Complete")
                    : entry.state == Task::State::AbortedByUser ? tr("Cancelled") : tr("Failed");
                if (entry.finishedAt.isValid()) status += " · " + entry.finishedAt.toString("HH:mm");
            } else if (status.isEmpty()) status = tr("Waiting");
            auto* state = new QLabel(status, row);
            state->setTextFormat(Qt::PlainText);
            state->setWordWrap(true);
            state->setProperty("pxlFlowHint", true);
            state->setStyleSheet("font-size: 12px; color: #abb3bf;");
            body->addWidget(state);
            if (entry.active()) {
                auto* progress = new QProgressBar(row);
                progress->setTextVisible(false);
                progress->setRange(0, entry.total > 0 ? 1000 : 0);
                if (entry.total > 0) progress->setValue(int(std::clamp(1000.0 * entry.current / entry.total, 0.0, 1000.0)));
                body->addWidget(progress);
            }
            auto* item = new QListWidgetItem(list);
            item->setSizeHint(QSize(0, row->sizeHint().height()));
            list->setItemWidget(item, row);
        }
        clear->setVisible(completed);
        empty->setVisible(m_records.isEmpty());
        list->setVisible(!m_records.isEmpty());
        const int desiredHeight = m_records.isEmpty() ? 112 : qMin(520, 76 + int(m_records.size()) * 100);
        const auto screenSize = parent ? parent->screen()->availableGeometry().size() - QSize(16, 16) : QSize(400, 520);
        dialog->resize(qMin(m_records.isEmpty() ? 336 : 400, screenSize.width()), qMin(desiredHeight, screenSize.height()));
        if (parent) {
            const QRect screen = parent->screen()->availableGeometry().adjusted(8, 8, -8, -8);
            QPoint popupPosition = parent->mapToGlobal(QPoint(parent->width() + 8, 0));
            if (popupPosition.x() + dialog->width() > screen.right())
                popupPosition.setX(parent->mapToGlobal(QPoint(0, 0)).x() - dialog->width() - 8);
            popupPosition.setX(qBound(screen.left(), popupPosition.x(), qMax(screen.left(), screen.right() - dialog->width())));
            popupPosition.setY(qBound(screen.top(), popupPosition.y(), qMax(screen.top(), screen.bottom() - dialog->height())));
            dialog->move(popupPosition);
        }
        list->verticalScrollBar()->setValue(position);
        dialog->updateBackdrop();
    };
    auto* timer = new QTimer(dialog);
    timer->setSingleShot(true);
    timer->setInterval(100);
    connect(timer, &QTimer::timeout, dialog, refresh);
    connect(this, &TaskHistory::changed, dialog, [timer] { if (!timer->isActive()) timer->start(); });
    connect(clear, &QPushButton::clicked, this, &TaskHistory::clearFinished);
    refresh();
    dialog->show();
    Pxl::animatePage(dialog);
}
} // namespace Pxl

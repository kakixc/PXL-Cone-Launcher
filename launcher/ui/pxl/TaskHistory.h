// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QDateTime>
#include <QIcon>
#include <QList>
#include <QObject>
#include <QPointer>
#include <memory>
#include "tasks/Task.h"

class QDialog;
class QWidget;

namespace Pxl {
class TaskHistory final : public QObject {
    Q_OBJECT
   public:
    struct Record {
        quint64 id = 0;
        QString title;
        QString status;
        QString details;
        QString error;
        QStringList warnings;
        QIcon icon;
        qint64 current = 0;
        qint64 total = 0;
        Task::State state = Task::State::Inactive;
        QDateTime finishedAt;
        QPointer<Task> task;
        bool canAbort = false;
        bool active() const { return state == Task::State::Inactive || state == Task::State::Running; }
    };
    explicit TaskHistory(QObject* parent = nullptr);
    static TaskHistory* instance();
    void watch(Task* task, const QString& title, const QIcon& icon = {});
    // Own an asynchronous operation independently of the page that started it.
    void startOwned(std::unique_ptr<Task> task, const QString& title, const QIcon& icon = {});
    QList<Record> records() const { return m_records; }
    int activeCount() const;
    bool cancel(quint64 id);
    void clearFinished();
    void showTasks(QWidget* parent);
   signals:
    void changed();
   private:
    Record* record(quint64 id);
    void trimHistory();
    void notifyChanged();
    QList<Record> m_records;
    quint64 m_nextId = 0;
    QPointer<QDialog> m_dialog;
};
} // namespace Pxl

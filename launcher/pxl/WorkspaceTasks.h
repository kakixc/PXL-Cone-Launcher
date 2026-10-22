// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QFutureWatcher>
#include <functional>
#include "InstanceTask.h"
#include "WorkspaceTransfer.h"

namespace Pxl {

class FileOperationTask : public Task {
   public:
    using Operation = std::function<QString(std::atomic_bool*, Progress)>;
    FileOperationTask(QString status, Operation operation);
    ~FileOperationTask() override;
    bool abort() override;

   protected:
    void executeTask() override;

   private:
    Operation m_operation;
    std::atomic_bool m_cancel{ false };
    QFutureWatcher<QString> m_watcher;
};

class WorkspaceImportTask : public InstanceTask {
   public:
    // Empty archive means copy a Prism/Pinecone source workspace.
    WorkspaceImportTask(Workspace workspace, QString archive = {}, int index = 0);
    ~WorkspaceImportTask() override;
    bool abort() override;
    // Call only after InstanceList has successfully published the staging area.
    void markPublished() { m_published = true; }

   protected:
    void executeTask() override;

   private:
    void workerFinished();
    void downloadContent();
    void verifyContent();
    void finishImport();
    Progress progressCallback();

    Workspace m_workspace;
    QString m_archive;
    int m_index;
    std::atomic_bool m_cancel{ false };
    QFutureWatcher<QString> m_watcher;
    Task::Ptr m_download;
    bool m_verifying = false;
    bool m_published = false;
    QString m_installedIcon;
};
}  // namespace Pxl

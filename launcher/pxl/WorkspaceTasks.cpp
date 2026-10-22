// SPDX-License-Identifier: GPL-3.0-only
#include "WorkspaceTasks.h"

#include <QDir>
#include <QElapsedTimer>
#include <QImageReader>
#include <QUuid>
#include <QtConcurrentRun>
#include "Application.h"
#include "icons/IconList.h"
#include "net/ApiDownload.h"
#include "net/ChecksumValidator.h"
#include "net/NetJob.h"
#include "settings/INIFile.h"

namespace Pxl {
namespace {
class ContentValidator : public Net::ChecksumValidator {
   public:
    ContentValidator(const ContentFile& file) : ChecksumValidator(QCryptographicHash::Sha256, file.sha256), m_expected(file.size) {}
    bool init(QNetworkRequest& request) override
    {
        m_received = 0;
        return ChecksumValidator::init(request);
    }
    bool write(QByteArray& data) override
    {
        if (data.size() > m_expected - m_received) return false;
        m_received += data.size();
        return ChecksumValidator::write(data);
    }
    bool validate(QNetworkReply& reply) override
    {
        return m_received == m_expected && ChecksumValidator::validate(reply);
    }
   private:
    qint64 m_expected;
    qint64 m_received = 0;
};

Progress queuedProgress(Task* task)
{
    auto last = std::make_shared<QElapsedTimer>();
    last->start();
    return [task, last](qint64 value, qint64 total, const QString& detail) {
        if (value != total && !last->hasExpired(50)) return;
        last->restart();
        QMetaObject::invokeMethod(task, [task, value, total, detail] {
            task->setProgress(value, total);
            task->setDetails(detail);
        }, Qt::QueuedConnection);
    };
}
}  // namespace

FileOperationTask::FileOperationTask(QString status, Operation operation) : m_operation(std::move(operation))
{
    setStatus(status);
    setAbortable(true);
    connect(&m_watcher, &QFutureWatcher<QString>::finished, this, [this] {
        const QString error = m_watcher.result();
        if (m_cancel.load()) emitAborted();
        else if (!error.isEmpty()) emitFailed(error);
        else emitSucceeded();
    });
}

FileOperationTask::~FileOperationTask()
{
    m_cancel.store(true);
    m_watcher.waitForFinished();
}

void FileOperationTask::executeTask()
{
    auto progress = queuedProgress(this);
    m_watcher.setFuture(QtConcurrent::run([this, progress] { return m_operation(&m_cancel, progress); }));
}

bool FileOperationTask::abort()
{
    if (!isRunning()) return false;
    m_cancel.store(true);
    setAbortable(false);
    return true;  // Completion is emitted only once the worker stops touching staging.
}

WorkspaceImportTask::WorkspaceImportTask(Workspace workspace, QString archive, int index)
    : m_workspace(std::move(workspace)), m_archive(std::move(archive)), m_index(index)
{
    setName(m_workspace.name);
    setGroup(m_workspace.group);
    setIcon("default");
    connect(&m_watcher, &QFutureWatcher<QString>::finished, this, &WorkspaceImportTask::workerFinished);
}

WorkspaceImportTask::~WorkspaceImportTask()
{
    m_cancel.store(true);
    m_watcher.waitForFinished();
    if (!m_published && !m_installedIcon.isEmpty())
        QFile::remove(m_installedIcon);  // This operation's generated UUID icon only.
}

Progress WorkspaceImportTask::progressCallback() { return queuedProgress(this); }

void WorkspaceImportTask::executeTask()
{
    setAbortable(true);
    setStatus(tr("Preparing %1").arg(name()));
    auto progress = progressCallback();
    m_watcher.setFuture(QtConcurrent::run([this, progress] {
        if (m_archive.isEmpty())
            return copyLauncherInstance(m_workspace, m_stagingPath, &m_cancel, progress);
        return prepareArchiveInstance(m_archive, m_index, m_stagingPath, m_workspace, &m_cancel, progress);
    }));
}

bool WorkspaceImportTask::abort()
{
    if (!isRunning()) return false;
    m_cancel.store(true);
    setAbortable(false);
    if (m_download && m_download->isRunning()) m_download->abort();
    return true;
}

void WorkspaceImportTask::workerFinished()
{
    const auto error = m_watcher.result();
    if (m_cancel.load()) { emitAborted(); return; }
    if (!error.isEmpty()) { emitFailed(error); return; }
    if (m_verifying || m_archive.isEmpty()) finishImport();
    else downloadContent();
}

void WorkspaceImportTask::downloadContent()
{
    auto job = makeShared<NetJob>(tr("Restoring exact pack content"), APPLICATION->network());
    bool pending = false;
    for (const auto& file : m_workspace.files) {
        if (file.bundled) continue;
        pending = true;
        auto download = Net::ApiDownload::makeFile(QUrl(file.url), QDir(m_stagingPath).filePath(file.path));
        download->addValidator(new ContentValidator(file));
        job->addNetAction(download);
    }
    if (!pending) { verifyContent(); return; }
    m_download = job;
    connect(job.get(), &Task::progress, this, &Task::setProgress);
    connect(job.get(), &Task::status, this, &Task::setStatus);
    connect(job.get(), &Task::succeeded, this, &WorkspaceImportTask::verifyContent);
    connect(job.get(), &Task::failed, this, [this](const QString& error) {
        if (m_cancel.load()) emitAborted();
        else emitFailed(tr("An exact content version could not be restored. No replacement was installed.\n%1").arg(error));
    });
    connect(job.get(), &Task::aborted, this, &WorkspaceImportTask::emitAborted);
    job->start();
}

void WorkspaceImportTask::verifyContent()
{
    if (m_cancel.load()) { emitAborted(); return; }
    m_verifying = true;
    setStatus(tr("Verifying restored content"));
    m_watcher.setFuture(QtConcurrent::run([this] { return verifyInstance(m_workspace, m_stagingPath, &m_cancel); }));
}

void WorkspaceImportTask::finishImport()
{
    if (m_cancel.load()) { emitAborted(); return; }
    setAbortable(false);
    INIFile cfg;
    if (!cfg.loadFile(QDir(m_stagingPath).filePath("instance.cfg"))) {
        emitFailed(tr("Cannot read restored instance settings."));
        return;
    }
    const auto iconPath = QDir(m_stagingPath).filePath("icon.png");
    if (QFileInfo::exists(iconPath)) {
        QImageReader reader(iconPath);
        reader.setDecideFormatFromContent(true);
        if (!reader.size().isValid() || reader.size().width() > 4096 || reader.size().height() > 4096) {
            logWarning(tr("Custom icon exceeds size limits; using the default icon."));
        } else {
            const auto image = reader.read();
            const auto converted = QDir(m_stagingPath).filePath("pxl-import-icon.png");
            if (!image.isNull() && image.save(converted, "PNG")) {
                m_instIcon = "Pxl_" + QUuid::createUuid().toString(QUuid::Id128);
                APPLICATION->icons()->installIcon(converted, m_instIcon + ".png");
                m_installedIcon = QDir(APPLICATION->icons()->getDirectory()).filePath(m_instIcon + ".png");
                QFile::remove(converted);
            } else logWarning(tr("Custom icon could not be read; using the default icon."));
        }
    }
    cfg.set("name", name());
    cfg.set("iconKey", m_instIcon);
    if (!cfg.saveFile(QDir(m_stagingPath).filePath("instance.cfg"))) {
        emitFailed(tr("Cannot save restored instance settings."));
        return;
    }
    emitSucceeded();
}
}  // namespace Pxl

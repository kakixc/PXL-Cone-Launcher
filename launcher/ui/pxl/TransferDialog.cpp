// SPDX-License-Identifier: GPL-3.0-only
#include "TransferDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QJsonArray>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

#include "Application.h"
#include "BaseInstance.h"
#include "InstanceList.h"
#include "icons/IconList.h"
#include "icons/IconUtils.h"
#include "pxl/WorkspaceTasks.h"
#include "ui/dialogs/ProgressDialog.h"

namespace Pxl {
TransferDialog::TransferDialog(Mode mode, QWidget* parent, const QString& selectedId, const QString& archive)
    : QDialog(parent), m_mode(mode), m_archive(archive)
{
    setWindowTitle(mode == Import ? tr("Import your Minecraft setup") : tr("Share or transfer your Minecraft setup"));
    resize(680, 560);
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(14);
    auto intro = new QLabel(mode == Import ? tr("Restore a PXL pack or copy instances from Prism / Pinecone.") :
                                            tr("Share one pack with a friend, or move several instances to another computer."), this);
    intro->setWordWrap(true);
    layout->addWidget(intro);
    if (mode == Import) {
        auto row = new QHBoxLayout;
        m_source = new QLineEdit(this);
        m_source->setAccessibleName(tr("Archive or launcher directory"));
        m_source->setPlaceholderText(tr("Select an archive or a launcher directory"));
        row->addWidget(m_source, 1);
        auto file = new QPushButton(tr("Open pack…"), this);
        auto folder = new QPushButton(tr("Launcher folder…"), this);
        row->addWidget(file);
        row->addWidget(folder);
        connect(file, &QPushButton::clicked, this, [this] { chooseSource(false); });
        connect(folder, &QPushButton::clicked, this, [this] { chooseSource(true); });
        connect(m_source, &QLineEdit::returnPressed, this, &TransferDialog::loadSource);
        layout->addLayout(row);
        const auto locations = launcherLocations();
        if (!locations.isEmpty()) {
            auto detected = new QComboBox(this);
            detected->setAccessibleName(tr("Detected launchers"));
            detected->addItem(tr("Detected launchers — choose to preview"), QString());
            for (const auto& location : locations) detected->addItem(location, location);
            connect(detected, &QComboBox::activated, this, [this, detected](int index) {
                if (index == 0) return;
                m_source->setText(detected->itemData(index).toString());
                loadSource();
            });
            layout->addWidget(detected);
        }
    } else {
        auto row = new QHBoxLayout;
        m_format = new QComboBox(this);
        m_format->addItem(tr("Send one pack to a friend (.pxlconep)"), "pxlconep");
        m_format->addItem(tr("Move instances to another computer (.pxlt)"), "pxlt");
        m_format->setAccessibleName(tr("Export purpose"));
        m_storage = new QComboBox(this);
        m_storage->addItem(tr("Small — download exact known content on restore"), "smart");
        m_storage->addItem(tr("Offline — include selected content files"), "offline");
        m_storage->setAccessibleName(tr("Content storage"));
        row->addWidget(m_format, 1);
        row->addWidget(m_storage, 1);
        layout->addLayout(row);
        m_worlds = new QCheckBox(tr("Include worlds"), this);
        m_screenshots = new QCheckBox(tr("Include screenshots"), this);
        auto options = new QHBoxLayout;
        options->addWidget(m_worlds);
        options->addWidget(m_screenshots);
        options->addStretch();
        layout->addLayout(options);
        for (int i = 0; i < APPLICATION->instances()->count(); ++i) {
            const auto instance = APPLICATION->instances()->at(i);
            Workspace workspace;
            workspace.name = instance->name();
            workspace.root = instance->instanceRoot();
            workspace.group = APPLICATION->instances()->getInstanceGroup(instance->id());
            workspace.iconPath = IconUtils::findBestIconIn(APPLICATION->icons()->getDirectory(), instance->iconKey());
            // Running instances remain visible but cannot be exported inconsistently.
            workspace.settings["pxl-running"] = instance->isRunning();
            workspace.settings["pxl-selected"] = instance->id() == selectedId;
            m_workspaces.append(workspace);
        }
        connect(m_format, &QComboBox::currentIndexChanged, this, &TransferDialog::updateSummary);
        connect(m_storage, &QComboBox::currentIndexChanged, this, &TransferDialog::updateSummary);
        connect(m_worlds, &QCheckBox::toggled, this, &TransferDialog::updateSummary);
        connect(m_screenshots, &QCheckBox::toggled, this, &TransferDialog::updateSummary);
    }
    m_list = new QListWidget(this);
    m_list->setAccessibleName(tr("Instances to import or export"));
    m_list->setSelectionMode(QAbstractItemView::NoSelection);
    layout->addWidget(m_list, 1);
    connect(m_list, &QListWidget::itemChanged, this, &TransferDialog::updateSummary);
    m_summary = new QLabel(this);
    m_summary->setWordWrap(true);
    m_summary->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_summary);
    if (mode == Import) {
        auto details = new QPushButton(tr("View selected content…"), this);
        connect(details, &QPushButton::clicked, this, [this] {
            QDialog preview(this);
            preview.setWindowTitle(tr("Import content preview"));
            preview.resize(720, 500);
            auto box = new QVBoxLayout(&preview);
            auto text = new QPlainTextEdit(&preview);
            text->setReadOnly(true);
            QStringList lines;
            for (int i = 0; i < m_list->count(); ++i) {
                if (m_list->item(i)->checkState() != Qt::Checked) continue;
                const auto& workspace = m_workspaces[i];
                lines.append(workspace.name);
                for (const auto& component : workspace.components["components"].toArray())
                    lines.append(component.toObject()["uid"].toString() + " " + component.toObject()["version"].toString());
                if (workspace.files.isEmpty())
                    lines.append(tr("Copy local content from: %1\nWorlds: %2\nScreenshots: %3").arg(workspace.root,
                        workspace.worldsPresent ? tr("present") : tr("none found"),
                        workspace.screenshotsPresent ? tr("present") : tr("none found")));
                for (const auto& file : workspace.files)
                    lines.append(tr("%1 — %2 — %3 bytes").arg(file.path, file.bundled ? tr("included") :
                        tr("download %1 / %2").arg(file.projectId, file.versionId)).arg(file.size));
                lines.append(QString());
            }
            text->setPlainText(lines.join('\n'));
            box->addWidget(text);
            auto close = new QDialogButtonBox(QDialogButtonBox::Close, &preview);
            connect(close, &QDialogButtonBox::rejected, &preview, &QDialog::reject);
            box->addWidget(close);
            preview.exec();
        });
        layout->addWidget(details);
    }
    auto privacy = new QLabel(mode == Import ?
        tr("Imports create new instances. Original files stay in place. Close games in the source launcher before copying. "
           "Sign in again in this launcher; account credentials and custom launch commands are not imported.") :
        tr("Unknown local content is included. Worlds are private: include them only when intended. "
           "Minecraft, loaders and Java may still need downloading on the receiving computer."), this);
    privacy->setWordWrap(true);
    layout->addWidget(privacy);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_run = buttons->addButton(mode == Import ? tr("Import selected") : tr("Export selected…"), QDialogButtonBox::AcceptRole);
    if (m_source) {
        connect(m_source, &QLineEdit::textChanged, this, [this](const QString& source) {
            if (source.trimmed() == m_previewSource) return;
            m_workspaces.clear();
            populate();
            m_summary->setText(tr("Press Enter or choose a source to load a new preview."));
        });
    }
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_run, &QPushButton::clicked, this, &TransferDialog::runOperation);
    layout->addWidget(buttons);
    populate();
    if (!archive.isEmpty() && m_source) {
        m_source->setText(archive);
        QTimer::singleShot(0, this, &TransferDialog::loadSource);
    }
}

void TransferDialog::populate()
{
    const QSignalBlocker blocker(m_list);
    m_list->clear();
    for (const auto& workspace : m_workspaces) {
        QString label = workspace.name;
        if (m_mode == Import && !workspace.components.isEmpty())
            label += tr(" — Minecraft %1 · %2").arg(workspace.minecraftVersion.isEmpty() ? tr("unknown version") : workspace.minecraftVersion,
                                                   workspace.worldsPresent ? tr("worlds included") : tr("no worlds"));
        auto item = new QListWidgetItem(label, m_list);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(m_mode == Import || workspace.settings["pxl-selected"].toBool() ? Qt::Checked : Qt::Unchecked);
        const bool unavailable = m_mode == Import ? workspace.components.isEmpty() : workspace.settings["pxl-running"].toBool();
        if (unavailable) {
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
            item->setCheckState(Qt::Unchecked);
            item->setText(workspace.name + (m_mode == Import ? tr(" — unsupported or unreadable instance") : tr(" — running; stop before exporting")));
        }
        item->setToolTip(workspace.root.isEmpty() ? workspace.group : workspace.root);
    }
    updateSummary();
}

void TransferDialog::chooseSource(bool folder)
{
    const auto path = folder ? QFileDialog::getExistingDirectory(this, tr("Choose Prism / Pinecone data or instances folder")) :
        QFileDialog::getOpenFileName(this, tr("Open PXL pack or transfer"), {}, tr("PXL archives (*.pxlconep *.pxlt)"));
    if (path.isEmpty()) return;
    m_source->setText(path);
    loadSource();
}

void TransferDialog::loadSource()
{
    const auto source = m_source->text().trimmed();
    if (source.isEmpty()) return;
    auto result = std::make_shared<QVector<Workspace>>();
    auto manifest = std::make_shared<Manifest>();
    const bool folder = QFileInfo(source).isDir();
    FileOperationTask task(tr("Reading import preview"), [source, folder, result, manifest](std::atomic_bool* cancel, Progress) {
        if (folder) return scanLauncher(source, *result, cancel);
        const auto error = inspectArchive(source, *manifest, cancel);
        if (error.isEmpty()) *result = manifest->instances;
        return error;
    });
    ProgressDialog progress(this);
    if (progress.execWithTask(&task) != QDialog::Accepted) {
        if (!task.wasSuccessful() && task.getState() != Task::State::AbortedByUser)
            QMessageBox::warning(this, tr("Cannot read import"), task.failReason());
        m_workspaces.clear();
        populate();
        return;
    }
    m_archive = folder ? QString() : source;
    m_previewSource = source;
    m_workspaces = *result;
    populate();
}

void TransferDialog::updateSummary()
{
    int selected = 0, bundled = 0, downloads = 0;
    qint64 bytes = 0;
    for (int i = 0; i < m_list->count(); ++i) {
        if (m_list->item(i)->checkState() != Qt::Checked) continue;
        ++selected;
        for (const auto& file : m_workspaces[i].files) {
            bytes += file.size;
            if (file.bundled) ++bundled;
            else ++downloads;
        }
    }
    const bool singlePack = m_mode == Export && m_format->currentData().toString() == "pxlconep";
    m_run->setEnabled(selected > 0 && (!singlePack || selected == 1));
    QString summary = tr("%n instance(s) selected.", "", selected);
    if (singlePack && selected > 1) summary += tr(" Select one instance for a pack, or choose a transfer for multiple instances.");
    if (m_mode == Import && !m_archive.isEmpty()) {
        summary += tr("\n%1 bundled files · %2 exact downloads · %3 MiB installed content.")
                       .arg(bundled).arg(downloads).arg(bytes / (1024 * 1024));
        summary += tr("\nUse “View selected content” to inspect the complete file list.");
        // The list tooltip is supplemented with a selectable detailed preview.
        for (int i = 0; i < m_list->count(); ++i) {
            if (m_list->item(i)->checkState() != Qt::Checked) continue;
            QStringList lines;
            for (const auto& file : m_workspaces[i].files)
                lines.append(tr("%1 — %2").arg(file.path, file.bundled ? tr("included") : tr("exact download")));
            m_list->item(i)->setToolTip(lines.join('\n'));
        }
    }
    if (m_mode == Export) summary += tr("\nThe next step chooses the archive destination; exports do not change your instances.");
    m_summary->setText(summary);
}

void TransferDialog::runOperation()
{
    if (m_mode == Import && m_source->text().trimmed() != m_previewSource) return;
    QVector<int> selected;
    for (int i = 0; i < m_list->count(); ++i)
        if (m_list->item(i)->checkState() == Qt::Checked) selected.append(i);
    if (selected.isEmpty()) return;
    if (m_mode == Export) {
        const auto format = m_format->currentData().toString();
        const auto mode = m_storage->currentData().toString() == "offline" ? QString("offline") : (format == "pxlt" ? QString("thin") : QString("smart"));
        if (format == "pxlconep" && selected.size() != 1) return;
        const auto path = QFileDialog::getSaveFileName(this, tr("Save archive"), "Minecraft." + format,
                                                      tr("PXL archive (*.%1)").arg(format));
        if (path.isEmpty()) return;
        QVector<Workspace> instances;
        for (const auto index : selected) {
            const auto& workspace = m_workspaces[index];
            // The main-window model can change while the export dialog is open.
            for (int i = 0; i < APPLICATION->instances()->count(); ++i) {
                const auto current = APPLICATION->instances()->at(i);
                if (current->instanceRoot() == workspace.root && current->isRunning()) {
                    QMessageBox::warning(this, tr("Instance is running"), tr("Stop Minecraft before exporting this instance."));
                    return;
                }
            }
            instances.append(workspace);
        }
        const ExportOptions options{ format, mode, m_worlds->isChecked(), m_screenshots->isChecked() };
        FileOperationTask task(tr("Creating Minecraft archive"), [path, instances, options](std::atomic_bool* cancel, Progress progress) {
            return exportArchive(path, instances, options, cancel, progress);
        });
        ProgressDialog progress(this);
        if (progress.execWithTask(&task) == QDialog::Accepted) {
            QMessageBox::information(this, tr("Archive ready"), tr("Saved to:\n%1").arg(path));
            accept();
        }
        return;
    }
    QStringList results;
    bool failed = false;
    for (int position = 0; position < selected.size(); ++position) {
        const int index = selected[position];
        const auto workspace = m_workspaces[index];
        auto child = new WorkspaceImportTask(workspace, m_archive, index);
        std::unique_ptr<Task> task(APPLICATION->instances()->wrapInstanceTask(child));
        ProgressDialog progress(this);
        progress.execWithTask(task.get());
        if (task->wasSuccessful()) {
            child->markPublished();
            results.append(tr("Imported: %1").arg(workspace.name));
            m_list->item(index)->setCheckState(Qt::Unchecked);
            m_list->item(index)->setFlags(m_list->item(index)->flags() & ~Qt::ItemIsEnabled);
        } else {
            failed = true;
            results.append(tr("Not imported: %1\n%2").arg(workspace.name, task->failReason().isEmpty() ? tr("Cancelled") : task->failReason()));
            if (task->getState() == Task::State::AbortedByUser) break;
        }
    }
    QMessageBox::information(this, tr("Import results"), results.join("\n\n"));
    if (!failed) accept();
}
}  // namespace Pxl

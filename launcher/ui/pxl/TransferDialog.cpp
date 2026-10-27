// SPDX-License-Identifier: GPL-3.0-only
#include "TransferDialog.h"
#include "DialogOverlay.h"
#include "MessagePanel.h"
#include "CheckListDelegate.h"
#include "LibraryControls.h"

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
    setProperty("pxlFlow", true);
    setProperty("pxlPreferredSize", QSize(760, 680));
    resize(680, 560);
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(14);
    auto* title = new QLabel(windowTitle(), this);
    title->setProperty("pxlFlowTitle", true);
    layout->addWidget(title);
    auto intro = new QLabel(mode == Import ? tr("Restore a PXL pack or copy instances from Prism / Pinecone.") :
                                            tr("Share one pack with a friend, or move several instances to another computer."), this);
    intro->setWordWrap(true);
    intro->setProperty("pxlFlowHint", true);
    layout->addWidget(intro);
    if (mode == Import) {
        m_sourceChoices = new QWidget(this);
        auto row = new QVBoxLayout(m_sourceChoices);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(10);
        m_source = new QLineEdit(this);
        m_source->setAccessibleName(tr("Archive or launcher directory"));
        m_source->setPlaceholderText(tr("Select an archive or a launcher directory"));
        m_source->hide();
        auto sourceButton = [this](const QString& titleText, const QString& hint, const QString& iconName) {
            auto* button = new QPushButton(this);
            button->setAutoDefault(false);
            button->setAccessibleName(titleText + ". " + hint);
            button->setMinimumHeight(76);
            button->setStyleSheet("QPushButton { background: rgba(255,255,255,10); border: 1px solid rgba(213,221,233,24); border-radius: 12px; }"
                                 "QPushButton:hover { background: rgba(255,255,255,20); border-color: rgba(242,163,101,100); }");
            button->setCursor(Qt::PointingHandCursor);
            auto* body = new QHBoxLayout(button);
            body->setContentsMargins(16, 14, 16, 14);
            body->setSpacing(12);
            auto* icon = new QLabel(button);
            icon->setPixmap(QIcon(":/pxl/icons/" + iconName + ".svg").pixmap(28, 28));
            body->addWidget(icon);
            auto* copy = new QVBoxLayout;
            copy->setSpacing(5);
            auto* name = new QLabel(titleText, button);
            name->setStyleSheet("font-size: 15px; font-weight: 600; background: transparent;");
            auto* description = new QLabel(hint, button);
            description->setWordWrap(true);
            description->setStyleSheet("font-size: 12px; font-weight: 400; color: #a4adba; background: transparent;");
            copy->addWidget(name);
            copy->addWidget(description);
            body->addLayout(copy, 1);
            for (auto* label : {icon, name, description}) label->setAttribute(Qt::WA_TransparentForMouseEvents);
            return button;
        };
        auto* file = sourceButton(tr("Import a pack"), tr("Choose a .pxlconep or .pxlt archive"), "import");
        auto* folder = sourceButton(tr("From another launcher"), tr("Select a Prism or Pinecone folder"), "squares-four");
        row->addWidget(file);
        row->addWidget(folder);
        connect(file, &QPushButton::clicked, this, [this] { chooseSource(false); });
        connect(folder, &QPushButton::clicked, this, [this] { chooseSource(true); });
        connect(m_source, &QLineEdit::returnPressed, this, &TransferDialog::loadSource);
        layout->addWidget(m_sourceChoices);
        layout->addWidget(m_source);
        m_changeSource = new QPushButton(tr("Choose another source"), this);
        m_changeSource->setProperty("pxlQuiet", true);
        layout->addWidget(m_changeSource, 0, Qt::AlignLeft);
        connect(m_changeSource, &QPushButton::clicked, this, [this] {
            m_archive.clear();
            m_previewSource.clear();
            m_source->clear();
            m_workspaces.clear();
            populate();
        });
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
        m_format = new Pxl::ChoiceChip(this);
        m_format->addItem(tr("Share a pack"), "pxlconep");
        m_format->addItem(tr("Move to another PC"), "pxlt");
        m_format->setAccessibleName(tr("Export purpose"));
        m_storage = new Pxl::ChoiceChip(this);
        m_storage->addItem(tr("Compact archive"), "smart");
        m_storage->addItem(tr("Include selected files"), "offline");
        m_storage->setAccessibleName(tr("Content storage"));
        row->addWidget(m_format, 1);
        row->addWidget(m_storage, 1);
        layout->addLayout(row);
        m_worlds = new QCheckBox(tr("Include worlds"), this);
        m_screenshots = new QCheckBox(tr("Include screenshots"), this);
        for (auto* option : {m_worlds,m_screenshots})
            option->setStyleSheet("QCheckBox { spacing: 8px; padding: 6px 0; } QCheckBox::indicator { width: 18px; height: 18px; border: 1px solid #8c97a6; border-radius: 5px; background: #20252c; } QCheckBox::indicator:checked { background: #f2a365; image: url(:/pxl/icons/check.svg); }");
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
    m_list->setObjectName("transferInstances");
    m_list->setItemDelegate(new Pxl::CheckListDelegate(m_list));
    m_list->setMouseTracking(true);
    m_list->setStyleSheet("QListWidget { background: transparent; border: none; padding: 0; outline: none; }");
    auto listPalette = m_list->viewport()->palette();
    listPalette.setColor(QPalette::Base, QColor("#25282e"));
    listPalette.setColor(QPalette::Window, QColor("#25282e"));
    m_list->viewport()->setPalette(listPalette);
    m_list->viewport()->setAutoFillBackground(false);
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
        m_details = details;
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
            Pxl::DialogOverlay::run(&preview, this);
        });
        layout->addWidget(details);
    }
    auto privacy = new QLabel(mode == Import ?
        tr("Imports create new instances. Original files stay in place. Close games in the source launcher before copying. "
           "Sign in again in this launcher; account credentials and custom launch commands are not imported.") :
        tr("Unknown local content is included. Worlds are private: include them only when intended. "
           "Minecraft, loaders and Java may still need downloading on the receiving computer."), this);
    privacy->setWordWrap(true);
    privacy->setProperty("pxlFlowHint", true);
    m_privacy = privacy;
    layout->addWidget(privacy);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    m_run = buttons->addButton(mode == Import ? tr("Import selected") : tr("Export selected…"), QDialogButtonBox::AcceptRole);
    m_run->setObjectName("transferRun");
    m_run->setDefault(false);
    m_run->setAutoDefault(false);
    m_run->setProperty("pxlPrimary", true);
    if (m_source) {
        connect(m_source, &QLineEdit::textChanged, this, [this](const QString& source) {
            if (source.trimmed() == m_previewSource) return;
            m_previewSource.clear();
            m_archive.clear();
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
        const auto icon = QIcon(workspace.iconPath);
        item->setIcon(icon.isNull() ? QIcon(":/pxl/cone-icon.svg") : icon);
        item->setSizeHint(QSize(0, 58));
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
            showNotice(this, tr("Cannot read import"), task.failReason());
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
    if (m_mode == Import) {
        const bool hasRows = !m_workspaces.isEmpty();
        m_sourceChoices->setVisible(!hasRows);
        m_changeSource->setVisible(hasRows);
        m_run->setVisible(hasRows);
        m_list->setVisible(hasRows);
        m_source->setVisible(!m_source->text().isEmpty());
        m_details->setVisible(hasRows);
        m_details->setEnabled(selected > 0);
        m_privacy->setVisible(hasRows);
        setProperty("pxlPreferredSize", hasRows ? QSize(760, 700) : QSize(620, 440));
        QCoreApplication::postEvent(this, new QEvent(QEvent::LayoutRequest));
    }
    QString summary = tr("%n instance(s) selected.", "", selected);
    if (m_mode == Import && m_workspaces.isEmpty())
        summary = m_previewSource.isEmpty() ? tr("Choose a source above. Your original instances stay in place.") : tr("No instances were found in this source. Choose another archive or folder.");
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
    if (m_mode == Export)
        m_summary->setText(summary + tr("\nFormat: %1").arg(singlePack ? ".pxlconep" : ".pxlt"));
    m_list->setMaximumHeight(320);
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
                    showNotice(this, tr("Instance is running"), tr("Stop Minecraft before exporting this instance."));
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
            showNotice(this, tr("Archive ready"), tr("Saved to:\n%1").arg(path));
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
    showNotice(this, tr("Import results"), results.join("\n\n"));
    if (!failed) accept();
}
}  // namespace Pxl

// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2023 TheKodeToad <TheKodeToad@proton.me>
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
 */

#include "DataPackPage.h"
#include "minecraft/PackProfile.h"
#include "ui_ExternalResourcesPage.h"

#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/ProgressDialog.h"
#include "ui/dialogs/ResourceDownloadDialog.h"
#include "ui/dialogs/ResourceUpdateDialog.h"
#include "icons/IconList.h"
#include "ui/pxl/DialogOverlay.h"
#include "ui/pxl/InstanceCovers.h"
#include "ui/pxl/MessagePanel.h"

namespace {
void showDataPackNotice(QWidget* page, const QString& title, const QString& message, QMessageBox::Icon severity)
{
    if (page->property("pxlInstalledContent").toBool()) {
        QTimer::singleShot(0, page, [page, title, message] { Pxl::showNotice(page->window(), title, message); });
    } else {
        CustomMessageBox::selectable(page, title, message, severity)->show();
    }
}
}

DataPackPage::DataPackPage(BaseInstance* instance, DataPackFolderModel* model, QWidget* parent)
    : ExternalResourcesPage(instance, model, parent), m_model(model)
{
    ui->actionDownloadItem->setText(tr("Download Packs"));
    ui->actionDownloadItem->setToolTip(tr("Download data packs from online mod platforms"));
    ui->actionDownloadItem->setEnabled(true);
    ui->actionsToolbar->insertActionBefore(ui->actionAddItem, ui->actionDownloadItem);

    connect(ui->actionDownloadItem, &QAction::triggered, this, &DataPackPage::downloadDataPacks);

    ui->actionUpdateItem->setToolTip(tr("Try to check or update all selected data packs (all data packs if none are selected)"));
    connect(ui->actionUpdateItem, &QAction::triggered, this, &DataPackPage::updateDataPacks);
    ui->actionsToolbar->insertActionBefore(ui->actionAddItem, ui->actionUpdateItem);

    auto* updateMenu = new QMenu(this);

    auto* update = updateMenu->addAction(ui->actionUpdateItem->text());
    connect(update, &QAction::triggered, this, &DataPackPage::updateDataPacks);

    updateMenu->addAction(ui->actionResetItemMetadata);
    connect(ui->actionResetItemMetadata, &QAction::triggered, this, &DataPackPage::deleteDataPackMetadata);

    ui->actionUpdateItem->setMenu(updateMenu);

    ui->actionChangeVersion->setToolTip(tr("Change a data pack's version."));
    connect(ui->actionChangeVersion, &QAction::triggered, this, &DataPackPage::changeDataPackVersion);
    ui->actionsToolbar->insertActionAfter(ui->actionUpdateItem, ui->actionChangeVersion);
}

void DataPackPage::updateFrame(const QModelIndex& current, [[maybe_unused]] const QModelIndex& previous)
{
    auto sourceCurrent = m_filterModel->mapToSource(current);
    int row = sourceCurrent.row();
    auto& dp = m_model->at(row);
    ui->frame->updateWithDataPack(dp);
}

void DataPackPage::downloadDataPacks()
{
    if (routePxlDownload()) return;
    if (m_instance->typeName() != "Minecraft") {
        return;  // this is a null instance or a legacy instance
    }

    m_downloadDialog = new ResourceDownload::DataPackDownloadDialog(this, m_model, m_instance);
    connect(this, &QObject::destroyed, m_downloadDialog, &QDialog::close);
    connect(m_downloadDialog, &QDialog::finished, this, &DataPackPage::downloadDialogFinished);

    m_downloadDialog->open();
}

void DataPackPage::downloadDialogFinished(int result)
{
    if (result != 0) {
        auto* tasks = new ConcurrentTask(tr("Download Data Packs"), APPLICATION->settings()->get("NumberOfConcurrentDownloads").toInt());
        connect(tasks, &Task::failed, this, [this, tasks](const QString& reason) {
            showDataPackNotice(this, tr("Error"), reason, QMessageBox::Critical);
            tasks->deleteLater();
        });
        connect(tasks, &Task::aborted, this, [this, tasks]() {
            showDataPackNotice(this, tr("Aborted"), tr("Download stopped by user."), QMessageBox::Information);
            tasks->deleteLater();
        });
        connect(tasks, &Task::succeeded, this, [this, tasks]() {
            QStringList warnings = tasks->warnings();
            if (warnings.count()) {
                showDataPackNotice(this, tr("Warnings"), warnings.join('\n'), QMessageBox::Warning);
            }

            tasks->deleteLater();
        });

        if (m_downloadDialog) {
            for (auto& task : m_downloadDialog->getTasks()) {
                tasks->addTask(task);
            }
        } else {
            qWarning() << "ResourceDownloadDialog vanished before we could collect tasks!";
        }

        ProgressDialog loadDialog(this);
        loadDialog.setSkipButton(true, tr("Abort"));
        loadDialog.execWithTask(tasks);

        m_model->update();
    }
    if (m_downloadDialog) {
        m_downloadDialog->deleteLater();
    }
}

void DataPackPage::updateDataPacks()
{
    if (m_instance->typeName() != "Minecraft") {
        return;  // this is a null instance or a legacy instance
    }

    if (m_model->rowCount() == 0) return;

    if (m_instance->globalSettings()->get("ModMetadataDisabled").toBool()) {
        const auto message = tr("Data pack updates are unavailable when metadata is disabled!");
        if (property("pxlInstalledContent").toBool()) showDataPackNotice(this, tr("Error"), message, QMessageBox::Critical);
        else QMessageBox::critical(this, tr("Error"), message);
        return;
    }
    if (m_instance != nullptr && m_instance->isRunning()) {
        const auto message = tr("Updating data packs while the game is running may cause pack duplication and game crashes.\n"
                                "The old files may not be deleted as they are in use.\n"
                                "Are you sure you want to do this?");
        auto response =
            property("pxlInstalledContent").toBool()
                ? (Pxl::messagePanel(window(), tr("Confirm Update"), message, tr("Update data packs"), true)
                       ? QMessageBox::Yes : QMessageBox::No)
                : CustomMessageBox::selectable(this, tr("Confirm Update"), message,
                                         QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                ->exec();

        if (response != QMessageBox::Yes) {
            return;
        }
    }
    auto selection = m_filterModel->mapSelectionToSource(ui->treeView->selectionModel()->selection()).indexes();

    auto modsList = m_model->selectedResources(selection);
    bool useAll = modsList.empty();
    if (useAll) {
        modsList = m_model->allResources();
    }
    if (modsList.isEmpty()) return;

    ResourceUpdateDialog updateDialog(this, m_instance, m_model, modsList, false, { ModPlatform::ModLoaderType::DataPack });
    updateDialog.checkCandidates();

    if (updateDialog.aborted()) {
        showDataPackNotice(this, tr("Aborted"), tr("The data pack updater was aborted!"), QMessageBox::Warning);
        return;
    }
    if (updateDialog.noUpdates()) {
        QString message{ tr("'%1' is up-to-date! :)").arg(modsList.front()->name()) };
        if (modsList.size() > 1) {
            if (useAll) {
                message = tr("All data packs are up-to-date! :)");
            } else {
                message = tr("All selected data packs are up-to-date! :)");
            }
        }
        showDataPackNotice(this, tr("Update checker"), message, QMessageBox::Information);
        return;
    }

    const bool pxlPresentation = property("pxlInstalledContent").toBool();
    if (pxlPresentation) {
        updateDialog.usePxlPresentation(m_instance->name(), APPLICATION->icons()->getIcon(m_instance->iconKey()),
                                       Pxl::instanceCoverPath(m_instance->instanceRoot()));
    }
    const int result = pxlPresentation ? Pxl::DialogOverlay::run(&updateDialog, window()) : updateDialog.exec();
    if (result == QDialog::Accepted) {
        auto* tasks = new ConcurrentTask("Download Data Packs", APPLICATION->settings()->get("NumberOfConcurrentDownloads").toInt());
        connect(tasks, &Task::failed, this, [this, tasks](const QString& reason) {
            showDataPackNotice(this, tr("Error"), reason, QMessageBox::Critical);
            tasks->deleteLater();
        });
        connect(tasks, &Task::aborted, this, [this, tasks]() {
            showDataPackNotice(this, tr("Aborted"), tr("Download stopped by user."), QMessageBox::Information);
            tasks->deleteLater();
        });
        connect(tasks, &Task::succeeded, this, [this, tasks]() {
            QStringList warnings = tasks->warnings();
            if (warnings.count()) {
                showDataPackNotice(this, tr("Warnings"), warnings.join('\n'), QMessageBox::Warning);
            }
            tasks->deleteLater();
        });

        for (const auto& task : updateDialog.getTasks()) {
            tasks->addTask(task);
        }

        ProgressDialog loadDialog(this);
        loadDialog.setSkipButton(true, tr("Abort"));
        loadDialog.execWithTask(tasks);

        m_model->update();
    }
}

void DataPackPage::deleteDataPackMetadata()
{
    auto selection = m_filterModel->mapSelectionToSource(ui->treeView->selectionModel()->selection()).indexes();
    auto selectionCount = m_model->selectedDataPacks(selection).length();
    if (selectionCount == 0) {
        return;
    }
    if (selectionCount > 1) {
        const auto message = tr("You are about to remove the metadata for %1 data packs.\nAre you sure?").arg(selectionCount);
        if (property("pxlInstalledContent").toBool()) {
            if (!Pxl::messagePanel(window(), tr("Confirm Removal"), message, tr("Remove metadata"), true)) return;
        } else {
            auto response = CustomMessageBox::selectable(this, tr("Confirm Removal"), message,
                                                         QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                                ->exec();
            if (response != QMessageBox::Yes) return;
        }
    }

    m_model->deleteMetadata(selection);
}

void DataPackPage::changeDataPackVersion()
{
    if (m_downloadDialog) return;
    if (m_instance->typeName() != "Minecraft") {
        return;  // this is a null instance or a legacy instance
    }

    if (m_instance->globalSettings()->get("ModMetadataDisabled").toBool()) {
        const auto message = tr("Data pack updates are unavailable when metadata is disabled!");
        if (property("pxlInstalledContent").toBool()) showDataPackNotice(this, tr("Error"), message, QMessageBox::Critical);
        else QMessageBox::critical(this, tr("Error"), message);
        return;
    }

    const QModelIndexList rows = ui->treeView->selectionModel()->selectedRows();

    if (rows.count() != 1) {
        return;
    }

    Resource& resource = m_model->at(m_filterModel->mapToSource(rows[0]).row());

    if (resource.metadata() == nullptr) {
        return;
    }

    if (property("pxlInstalledContent").toBool()) {
        m_downloadDialog = new ResourceDownload::DataPackDownloadDialog(this, m_model, m_instance, true);
        if (!m_downloadDialog->hasProviders()) {
            m_downloadDialog->deleteLater();
            showDataPackNotice(this, tr("Error"), tr("No content provider is available for this instance."), QMessageBox::Critical);
            return;
        }
        connect(this, &QObject::destroyed, m_downloadDialog, &QDialog::close);
        connect(m_downloadDialog, &QDialog::finished, this, &DataPackPage::downloadDialogFinished);
        m_downloadDialog->useEmbeddedPresentation();
        m_downloadDialog->setProperty("pxlVersionChange", true);
        m_downloadDialog->setResourceMetadata(resource.metadata());
        return;
    }

    ResourceDownload::DataPackDownloadDialog mdownload(this, m_model, m_instance, true);
    mdownload.setResourceMetadata(resource.metadata());
    if (mdownload.exec() != 0) {
        auto* tasks = new ConcurrentTask("Download Data Packs", APPLICATION->settings()->get("NumberOfConcurrentDownloads").toInt());
        connect(tasks, &Task::failed, [this, tasks](const QString& reason) {
            CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->show();
            tasks->deleteLater();
        });
        connect(tasks, &Task::aborted, [this, tasks]() {
            CustomMessageBox::selectable(this, tr("Aborted"), tr("Download stopped by user."), QMessageBox::Information)->show();
            tasks->deleteLater();
        });
        connect(tasks, &Task::succeeded, [this, tasks]() {
            QStringList warnings = tasks->warnings();
            if (warnings.count()) {
                CustomMessageBox::selectable(this, tr("Warnings"), warnings.join('\n'), QMessageBox::Warning)->show();
            }

            tasks->deleteLater();
        });

        for (auto& task : mdownload.getTasks()) {
            tasks->addTask(task);
        }

        ProgressDialog loadDialog(this);
        loadDialog.setSkipButton(true, tr("Abort"));
        loadDialog.execWithTask(tasks);

        m_model->update();
    }
}

GlobalDataPackPage::GlobalDataPackPage(MinecraftInstance* instance, QWidget* parent) : QWidget(parent), m_instance(instance)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    setLayout(layout);

    connect(instance->settings()->getSetting("GlobalDataPacksEnabled").get(), &Setting::SettingChanged, this, [this] {
        updateContent();
        if (m_container != nullptr) {
            m_container->refreshContainer();
        }
    });

    connect(instance->settings()->getSetting("GlobalDataPacksPath").get(), &Setting::SettingChanged, this,
            &GlobalDataPackPage::updateContent);
}

QString GlobalDataPackPage::displayName() const
{
    if (m_underlyingPage == nullptr) {
        return {};
    }

    return m_underlyingPage->displayName();
}

QIcon GlobalDataPackPage::icon() const
{
    if (m_underlyingPage == nullptr) {
        return {};
    }

    return m_underlyingPage->icon();
}

QString GlobalDataPackPage::helpPage() const
{
    if (m_underlyingPage == nullptr) {
        return {};
    }

    return m_underlyingPage->helpPage();
}

bool GlobalDataPackPage::shouldDisplay() const
{
    return m_instance->settings()->get("GlobalDataPacksEnabled").toBool();
}

bool GlobalDataPackPage::apply()
{
    return m_underlyingPage == nullptr || m_underlyingPage->apply();
}

void GlobalDataPackPage::openedImpl()
{
    if (m_underlyingPage != nullptr) {
        m_underlyingPage->openedImpl();
    }
}

void GlobalDataPackPage::closedImpl()
{
    if (m_underlyingPage != nullptr) {
        m_underlyingPage->closedImpl();
    }
}

void GlobalDataPackPage::updateContent()
{
    if (m_underlyingPage != nullptr) {
        if (m_container->selectedPage() == this) {
            m_underlyingPage->closedImpl();
        }

        m_underlyingPage->apply();

        layout()->removeWidget(m_underlyingPage);

        delete m_underlyingPage;
        m_underlyingPage = nullptr;
    }

    if (shouldDisplay()) {
        m_underlyingPage = new DataPackPage(m_instance, m_instance->dataPackList());
        m_underlyingPage->setParentContainer(m_container);
        m_underlyingPage->updateExtraInfo = [this](QString id, QString value) { updateExtraInfo(std::move(id), std::move(value)); };

        if (m_container->selectedPage() == this) {
            m_underlyingPage->openedImpl();
        }

        layout()->addWidget(m_underlyingPage);
    }
}
void GlobalDataPackPage::setParentContainer(BasePageContainer* container)
{
    BasePage::setParentContainer(container);
    updateContent();
}

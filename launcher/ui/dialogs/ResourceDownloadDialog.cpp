// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
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

#include "ResourceDownloadDialog.h"
#include <QList>

#include <QPushButton>
#include <QLabel>
#include <QPixmapCache>
#include <algorithm>
#include <utility>

#include "Application.h"
#include "ResourceDownloadTask.h"

#include "minecraft/PackProfile.h"
#include "minecraft/mod/ModFolderModel.h"
#include "minecraft/mod/ResourcePackFolderModel.h"
#include "minecraft/mod/ShaderPackFolderModel.h"
#include "minecraft/mod/TexturePackFolderModel.h"

#include "minecraft/mod/tasks/GetModDependenciesTask.h"
#include "modplatform/ModIndex.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/ProgressDialog.h"
#include "ui/dialogs/ReviewMessageBox.h"

#include "ui/pages/modplatform/ResourcePage.h"

#include "ui/pages/modplatform/flame/FlameResourcePages.h"
#include "ui/pages/modplatform/modrinth/ModrinthResourcePages.h"

#include "modplatform/flame/FlameAPI.h"
#include "modplatform/modrinth/ModrinthAPI.h"
#include "ui/widgets/PageContainer.h"
#include "ui/pxl/DialogOverlay.h"
#include <QComboBox>
#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QGridLayout>
#include "ui/pxl/LibraryControls.h"

namespace ResourceDownload {

ResourceDownloadDialog::ResourceDownloadDialog(QWidget* parent, ResourceFolderModel* baseModel, bool suppressInitialSearch)
    : QDialog(parent)
    , m_base_model(baseModel)
    , m_buttons(QDialogButtonBox::Help | QDialogButtonBox::Ok | QDialogButtonBox::Cancel)
    , m_vertical_layout(this)
    , m_suppressInitialSearch(suppressInitialSearch)
{
    setObjectName(QStringLiteral("ResourceDownloadDialog"));
    setProperty("pxlFlow", true);

    resize(static_cast<int>(std::max(0.5 * parent->width(), 400.0)), static_cast<int>(std::max(0.75 * parent->height(), 400.0)));

    setWindowIcon(QIcon(":/pxl/cone-icon.svg"));

// small margins look ugly on macOS on modal windows
#ifndef Q_OS_MACOS
    m_buttons.setContentsMargins(0, 0, 6, 6);
#endif
    // Bonk Qt over its stupid head and make sure it understands which button is the default one...
    // See: https://stackoverflow.com/questions/24556831/qbuttonbox-set-default-button
    auto* okButton = m_buttons.button(QDialogButtonBox::Ok);
    okButton->setEnabled(false);
    okButton->setDefault(true);
    okButton->setAutoDefault(true);
    okButton->setText(tr("Review and confirm"));
    okButton->setProperty("pxlPrimary", true);
    okButton->setShortcut(tr("Ctrl+Return"));

    auto* cancelButton = m_buttons.button(QDialogButtonBox::Cancel);
    cancelButton->setDefault(false);
    cancelButton->setAutoDefault(false);

    auto* helpButton = m_buttons.button(QDialogButtonBox::Help);
    helpButton->setDefault(false);
    helpButton->setAutoDefault(false);

    setWindowModality(Qt::WindowModal);
}

void ResourceDownloadDialog::accept()
{
    if (!m_embedded && !geometrySaveKey().isEmpty()) {
        APPLICATION->settings()->set(geometrySaveKey(), QString::fromUtf8(saveGeometry().toBase64()));
    }

    QDialog::accept();
}

void ResourceDownloadDialog::reject()
{
    if (m_embedded) {
        emit embeddedCloseRequested();
        return;
    }
    auto selected = getTasks();
    if (selected.count() > 0) {
        auto reply = CustomMessageBox::selectable(this, tr("Confirmation Needed"),
                                                  tr("You have %1 selected resources.\n"
                                                     "Are you sure you want to close this dialog?")
                                                      .arg(selected.count()),
                                                  QMessageBox::Question, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                         ->exec();
        if (reply != QMessageBox::Yes) {
            return;
        }
    }

    if (!geometrySaveKey().isEmpty()) {
        APPLICATION->settings()->set(geometrySaveKey(), QString::fromUtf8(saveGeometry().toBase64()));
    }

    QDialog::reject();
}

// NOTE: We can't have this in the ctor because PageContainer calls a virtual function, and so
// won't work with subclasses if we put it in this ctor.
void ResourceDownloadDialog::initializeContainer()
{
// small margins look ugly on macOS on modal windows
#ifndef Q_OS_MACOS
    layout()->setContentsMargins(24, 24, 24, 24);
#endif

    m_container = new PageContainer(this, {}, this);
    m_container->useConeStyle();
    m_container->setSizePolicy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Expanding);
    m_container->layout()->setContentsMargins(0, 0, 0, 0);
    m_vertical_layout.setSpacing(20);
    auto* heading = new QLabel(tr("Browse %1").arg(resourcesString()), this);
    heading->setProperty("pxlFlowTitle", true);
    m_vertical_layout.addWidget(heading);
    auto* hint = new QLabel(tr("Choose a provider, find compatible content and review your selection before installing."), this);
    hint->setProperty("pxlFlowHint", true);
    hint->setWordWrap(true);
    m_vertical_layout.addWidget(hint);
    m_vertical_layout.addWidget(m_container, 1);
    resize(qMax(width(), 1000), qMax(height(), 700));

    m_container->addButtons(&m_buttons);

    connect(m_container, &PageContainer::selectedPageChanged, this, &ResourceDownloadDialog::selectedPageChanged);
}

void ResourceDownloadDialog::connectButtons()
{
    auto* okButton = m_buttons.button(QDialogButtonBox::Ok);
    okButton->setToolTip(
        tr("Opens a new popup to review your selected %1 and confirm your selection. Shortcut: Ctrl+Return").arg(resourcesString()));
    connect(okButton, &QPushButton::clicked, this, &ResourceDownloadDialog::confirm);

    auto* cancelButton = m_buttons.button(QDialogButtonBox::Cancel);
    connect(cancelButton, &QPushButton::clicked, this, &ResourceDownloadDialog::reject);

    auto* helpButton = m_buttons.button(QDialogButtonBox::Help);
    connect(helpButton, &QPushButton::clicked, m_container, &PageContainer::help);
}

void ResourceDownloadDialog::useEmbeddedPresentation()
{
    m_embedded = true;
    setWindowFlags(Qt::Widget);
    setWindowModality(Qt::NonModal);
    setMinimumSize(0, 0);
    setStyleSheet("QDialog#ResourceDownloadDialog { background: transparent; }");
    m_vertical_layout.setContentsMargins(0, 0, 0, 0);
    m_vertical_layout.setSpacing(12);
    for (auto* label : findChildren<QLabel*>(QString(), Qt::FindDirectChildrenOnly)) label->hide();
    m_buttons.button(QDialogButtonBox::Cancel)->hide();
    m_buttons.button(QDialogButtonBox::Help)->hide();
    m_buttons.button(QDialogButtonBox::Ok)->setAutoDefault(false);
    m_buttons.button(QDialogButtonBox::Ok)->setDefault(false);
    m_container->setPageHeadingVisible(false);
    m_container->hidePageList();
    if (auto* grid = qobject_cast<QGridLayout*>(m_container->layout())) {
        grid->setRowStretch(1, 1);
        grid->setVerticalSpacing(0);
    }
    auto* sources = new QWidget(this);
    sources->setObjectName("catalogSourceRow");
    sources->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto* sourceRow = new QHBoxLayout(sources);
    sourceRow->setContentsMargins(0, 0, 0, 0);
    sourceRow->addWidget(new QLabel(tr("Source"), sources));
    auto* choice = new Pxl::ChoiceChip(sources);
    choice->setObjectName("contentProviderChoice");
    choice->setMinimumWidth(180);
    for (auto* page : m_container->getPages()) {
        choice->addItem(page->icon(), page->displayName(), page->id());
        if (auto* resourcePage = dynamic_cast<ResourcePage*>(page)) resourcePage->useCardPresentation();
    }
    sourceRow->addWidget(choice);
    sourceRow->addStretch();
    m_vertical_layout.insertWidget(0, sources);
    connect(choice, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, choice](int index) {
        selectPage(choice->itemData(index).toString());
    });
    connect(m_container, &PageContainer::selectedPageChanged, choice, [choice](BasePage*, BasePage* selected) {
        if (!selected) return;
        const QSignalBlocker block(choice);
        choice->setCurrentIndex(choice->findData(selected->id()));
    });
    if (auto* selected = m_container->selectedPage()) choice->setCurrentIndex(choice->findData(selected->id()));
}

bool ResourceDownloadDialog::hasProviders() const
{
    return !m_container->getPages().isEmpty();
}

void ResourceDownloadDialog::confirm()
{
    auto* confirmDialog = ReviewMessageBox::create(this, tr("Confirm %1 to download").arg(resourcesString()));
    confirmDialog->retranslateUi(resourcesString());

    QHash<QString, GetModDependenciesTask::PackDependencyExtraInfo> dependencyExtraInfo;
    QStringList depNames;
    if (auto task = getModDependenciesTask(); task) {
        connect(task.get(), &Task::failed, this,
                [this](const QString& reason) { CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->exec(); });

        auto weak = task.toWeakRef();
        connect(task.get(), &Task::succeeded, this, [this, weak]() {
            QStringList warnings;
            if (auto task = weak.lock()) {
                warnings = task->warnings();
            }
            if (warnings.count()) {
                CustomMessageBox::selectable(this, tr("Warnings"), warnings.join('\n'), QMessageBox::Warning)->exec();
            }
        });

        // Check for updates
        ProgressDialog progressDialog(this);
        progressDialog.setSkipButton(true, tr("Abort"));
        progressDialog.setWindowTitle(tr("Checking for dependencies..."));
        auto ret = progressDialog.execWithTask(task.get());

        // If the dialog was skipped / some download error happened
        if (ret == QDialog::DialogCode::Rejected) {
            QMetaObject::invokeMethod(this, "reject", Qt::QueuedConnection);
            return;
        }
        for (const auto& dep : task->getDependecies()) {
            addResource(dep->pack, dep->version, "dependency");
            depNames << dep->pack->name;
        }
        dependencyExtraInfo = task->getExtraInfo();
    }

    auto selected = getTasks();
    std::ranges::sort(selected, [](const DownloadTaskPtr& a, const DownloadTaskPtr& b) {
        return QString::compare(a->getName(), b->getName(), Qt::CaseInsensitive) < 0;
    });
    for (auto& task : selected) {
        auto extraInfo = dependencyExtraInfo.value(task->getPack()->addonId.toString());
        QPixmap artwork;
        QPixmapCache::find(task->getPack()->logoUrl, &artwork);
        confirmDialog->appendResource({ .name = task->getName(),
                                        .filename = task->getFilename(),
                                        .provider = ModPlatform::ProviderCapabilities::name(task->getProvider()),
                                        .required_by = extraInfo.required_by,
                                        .version_type = task->getVersion().version_type.toString(),
                                        .enabled = !extraInfo.maybe_installed,
                                        .icon = QIcon(artwork) });
    }

    if (m_embedded) confirmDialog->usePxlPresentation(property("pxlReviewInstanceName").toString(),
        qvariant_cast<QIcon>(property("pxlReviewInstanceIcon")), property("pxlReviewInstanceCover").toString());

    const int confirmation = m_embedded ? Pxl::DialogOverlay::run(confirmDialog, window()) : confirmDialog->exec();
    if (confirmation != 0) {
        auto deselected = confirmDialog->deselectedResources();
        for (auto* page : m_container->getPages()) {
            auto* res = static_cast<ResourcePage*>(page);
            for (const auto& name : deselected) {
                res->removeResourceFromPage(name);
            }
        }

        this->accept();
    } else {
        for (const auto& name : depNames) {
            removeResource(name);
        }
    }
}

bool ResourceDownloadDialog::selectPage(QString pageId)
{
    return m_container->selectPage(std::move(pageId));
}

ResourcePage* ResourceDownloadDialog::selectedPage()
{
    auto* result = dynamic_cast<ResourcePage*>(m_container->selectedPage());
    Q_ASSERT(result != nullptr);
    return result;
}

void ResourceDownloadDialog::addResource(ModPlatform::IndexedPack::Ptr pack, ModPlatform::IndexedVersion& ver, QString downloadReason)
{
    removeResource(pack->name);
    selectedPage()->addResourceToPage(pack, ver, getBaseModel(), std::move(downloadReason));
    setButtonStatus();
}

void ResourceDownloadDialog::removeResource(const QString& packName)
{
    for (auto* page : m_container->getPages()) {
        static_cast<ResourcePage*>(page)->removeResourceFromPage(packName);
    }
    setButtonStatus();
}

void ResourceDownloadDialog::setButtonStatus()
{
    auto selected = false;
    for (auto* page : m_container->getPages()) {
        auto* res = static_cast<ResourcePage*>(page);
        selected = selected || res->hasSelectedPacks();
    }
    m_buttons.button(QDialogButtonBox::Ok)->setEnabled(selected);
}

QList<ResourceDownloadDialog::DownloadTaskPtr> ResourceDownloadDialog::getTasks()
{
    QList<DownloadTaskPtr> selected;
    for (auto* page : m_container->getPages()) {
        auto* res = static_cast<ResourcePage*>(page);
        selected.append(res->selectedPacks());
    }
    return selected;
}

void ResourceDownloadDialog::selectedPageChanged(BasePage* previous, BasePage* selected)
{
    // If previous is null (first selection), nothing to sync
    if (!previous) {
        return;
    }

    auto* prevPage = dynamic_cast<ResourcePage*>(previous);
    if (!prevPage) {
        qCritical() << "Page '" << previous->displayName() << "' in ResourceDownloadDialog is not a ResourcePage!";
        return;
    }

    // Same effect as having a global search bar
    auto* result = dynamic_cast<ResourcePage*>(selected);
    Q_ASSERT(result != nullptr);
    result->setSearchTerm(prevPage->getSearchTerm());
}

ModDownloadDialog::ModDownloadDialog(QWidget* parent, ModFolderModel* mods, BaseInstance* instance, bool suppressInitialSearch)
    : ResourceDownloadDialog(parent, mods, suppressInitialSearch), m_instance(instance)
{
    setWindowTitle(dialogTitle());

    initializeContainer();
    connectButtons();

    if (!geometrySaveKey().isEmpty()) {
        restoreGeometry(QByteArray::fromBase64(APPLICATION->settings()->get(geometrySaveKey()).toString().toUtf8()));
    }
}

QList<BasePage*> ModDownloadDialog::getPages()
{
    QList<BasePage*> pages;

    auto loaders = static_cast<MinecraftInstance*>(m_instance)->getPackProfile()->getSupportedModLoaders().value();

    if (ModrinthAPI::validateModLoaders(loaders)) {
        auto* page = ModrinthModPage::create(this, *m_instance);
        page->setSuppressInitialSearch(m_suppressInitialSearch);
        pages.append(page);
    }
    if (APPLICATION->capabilities() & Application::SupportsFlame && FlameAPI::validateModLoaders(loaders)) {
        auto* page = FlameModPage::create(this, *m_instance);
        page->setSuppressInitialSearch(m_suppressInitialSearch);
        pages.append(page);
    }

    return pages;
}

GetModDependenciesTask::Ptr ModDownloadDialog::getModDependenciesTask()
{
    if (!APPLICATION->settings()->get("ModDependenciesDisabled").toBool()) {  // dependencies
        if (auto* model = dynamic_cast<ModFolderModel*>(getBaseModel()); model) {
            QList<std::shared_ptr<GetModDependenciesTask::PackDependency>> selectedVers;
            for (const auto& selected : getTasks()) {
                selectedVers.append(std::make_shared<GetModDependenciesTask::PackDependency>(selected->getPack(), selected->getVersion()));
            }

            return makeShared<GetModDependenciesTask>(m_instance, model, selectedVers);
        }
    }
    return nullptr;
}

ResourcePackDownloadDialog::ResourcePackDownloadDialog(QWidget* parent,
                                                       ResourcePackFolderModel* resourcePacks,
                                                       BaseInstance* instance,
                                                       bool suppressInitialSearch)
    : ResourceDownloadDialog(parent, resourcePacks, suppressInitialSearch), m_instance(instance)
{
    setWindowTitle(dialogTitle());

    initializeContainer();
    connectButtons();

    if (!geometrySaveKey().isEmpty()) {
        restoreGeometry(QByteArray::fromBase64(APPLICATION->settings()->get(geometrySaveKey()).toString().toUtf8()));
    }
}

QList<BasePage*> ResourcePackDownloadDialog::getPages()
{
    QList<BasePage*> pages;

    auto* modrinthPage = ModrinthResourcePackPage::create(this, *m_instance);
    modrinthPage->setSuppressInitialSearch(m_suppressInitialSearch);
    pages.append(modrinthPage);
    if (APPLICATION->capabilities() & Application::SupportsFlame) {
        auto* flamePage = FlameResourcePackPage::create(this, *m_instance);
        flamePage->setSuppressInitialSearch(m_suppressInitialSearch);
        pages.append(flamePage);
    }

    return pages;
}

TexturePackDownloadDialog::TexturePackDownloadDialog(QWidget* parent,
                                                     TexturePackFolderModel* resourcePacks,
                                                     BaseInstance* instance,
                                                     bool suppressInitialSearch)
    : ResourceDownloadDialog(parent, resourcePacks, suppressInitialSearch), m_instance(instance)
{
    setWindowTitle(dialogTitle());

    initializeContainer();
    connectButtons();

    if (!geometrySaveKey().isEmpty()) {
        restoreGeometry(QByteArray::fromBase64(APPLICATION->settings()->get(geometrySaveKey()).toString().toUtf8()));
    }
}

QList<BasePage*> TexturePackDownloadDialog::getPages()
{
    QList<BasePage*> pages;

    auto* modrinthPage = ModrinthTexturePackPage::create(this, *m_instance);
    modrinthPage->setSuppressInitialSearch(m_suppressInitialSearch);
    pages.append(modrinthPage);
    if (APPLICATION->capabilities() & Application::SupportsFlame) {
        auto* flamePage = FlameTexturePackPage::create(this, *m_instance);
        flamePage->setSuppressInitialSearch(m_suppressInitialSearch);
        pages.append(flamePage);
    }

    return pages;
}

ShaderPackDownloadDialog::ShaderPackDownloadDialog(QWidget* parent,
                                                   ShaderPackFolderModel* shaders,
                                                   BaseInstance* instance,
                                                   bool suppressInitialSearch)
    : ResourceDownloadDialog(parent, shaders, suppressInitialSearch), m_instance(instance)
{
    setWindowTitle(dialogTitle());

    initializeContainer();
    connectButtons();

    if (!geometrySaveKey().isEmpty()) {
        restoreGeometry(QByteArray::fromBase64(APPLICATION->settings()->get(geometrySaveKey()).toString().toUtf8()));
    }
}

QList<BasePage*> ShaderPackDownloadDialog::getPages()
{
    QList<BasePage*> pages;
    auto* modrinthPage = ModrinthShaderPackPage::create(this, *m_instance);
    modrinthPage->setSuppressInitialSearch(m_suppressInitialSearch);
    pages.append(modrinthPage);
    if (APPLICATION->capabilities() & Application::SupportsFlame) {
        auto* flamePage = FlameShaderPackPage::create(this, *m_instance);
        flamePage->setSuppressInitialSearch(m_suppressInitialSearch);
        pages.append(flamePage);
    }
    return pages;
}

void ResourceDownloadDialog::setResourceMetadata(const std::shared_ptr<Metadata::ModStruct>& meta)
{
    switch (meta->provider) {
        case ModPlatform::ResourceProvider::MODRINTH:
            selectPage(Modrinth::id());
            break;
        case ModPlatform::ResourceProvider::FLAME:
            selectPage(Flame::id());
            break;
    }

    setWindowTitle(tr("Change %1 version").arg(meta->name));
    m_container->hidePageList();
    m_buttons.hide();
    auto* page = selectedPage();
    page->openProject(meta->project_id);
}

DataPackDownloadDialog::DataPackDownloadDialog(QWidget* parent,
                                               DataPackFolderModel* dataPacks,
                                               BaseInstance* instance,
                                               bool suppressInitialSearch)
    : ResourceDownloadDialog(parent, dataPacks, suppressInitialSearch), m_instance(instance)
{
    setWindowTitle(dialogTitle());

    initializeContainer();
    connectButtons();

    if (!geometrySaveKey().isEmpty()) {
        restoreGeometry(QByteArray::fromBase64(APPLICATION->settings()->get(geometrySaveKey()).toByteArray()));
    }
}

QList<BasePage*> DataPackDownloadDialog::getPages()
{
    QList<BasePage*> pages;
    auto* modrinthPage = ModrinthDataPackPage::create(this, *m_instance);
    modrinthPage->setSuppressInitialSearch(m_suppressInitialSearch);
    pages.append(modrinthPage);
    if (APPLICATION->capabilities() & Application::SupportsFlame) {
        auto* flamePage = FlameDataPackPage::create(this, *m_instance);
        flamePage->setSuppressInitialSearch(m_suppressInitialSearch);
        pages.append(flamePage);
    }
    return pages;
}

}  // namespace ResourceDownload

// SPDX-FileCopyrightText: 2023 flowln <flowlnlnln@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-only AND Apache-2.0
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *  Copyright (C) 2023 TheKodeToad <TheKodeToad@proton.me>
 *  Copyright (c) 2023 Trial97 <alexandru.tripon97@gmail.com>
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

#include "ResourcePage.h"
#include "modplatform/ModIndex.h"
#include "ui_ResourcePage.h"

#include <StringUtils.h>
#include <QDesktopServices>
#include <QKeyEvent>
#include <QMessageBox>
#include <algorithm>
#include <utility>

#include "Markdown.h"

#include "Application.h"
#include "ui/dialogs/ResourceDownloadDialog.h"
#include "ui/pages/modplatform/ResourceModel.h"
#include "ui/widgets/ProjectItem.h"
#include "ui/pxl/CatalogCardDelegate.h"
#include "ui/pxl/DialogOverlay.h"
#include "ui/pxl/LibraryControls.h"
#include <QSignalBlocker>

namespace ResourceDownload {

ResourcePage::ResourcePage(ResourceDownloadDialog* parent, BaseInstance& baseInstance)
    : QWidget(parent), m_baseInstance(baseInstance), m_ui(new Ui::ResourcePage), m_parentDialog(parent), m_fetchProgress(this, false)
{
    m_ui->setupUi(this);
    m_ui->verticalLayout->setSpacing(16);
    m_ui->searchEdit->setPlaceholderText(tr("Search content"));
    m_ui->searchEdit->setClearButtonEnabled(true);
    m_ui->searchEdit->addAction(QIcon(":/pxl/icons/magnifying-glass.svg"), QLineEdit::LeadingPosition);
    m_ui->resourceFilterButton->setIcon(QIcon(":/pxl/icons/sliders-horizontal.svg"));
    m_ui->packView->setAlternatingRowColors(false);
    m_ui->packView->setSpacing(4);
    m_ui->splitter->setHandleWidth(16);

    m_ui->searchEdit->installEventFilter(this);

    m_ui->versionSelectionBox->view()->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_ui->versionSelectionBox->view()->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_ui->versionSelectionBox->view()->parentWidget()->setMaximumHeight(300);

    m_searchTimer.setTimerType(Qt::TimerType::CoarseTimer);
    m_searchTimer.setSingleShot(true);

    connect(&m_searchTimer, &QTimer::timeout, this, &ResourcePage::triggerSearch);

    // hide progress bar to prevent weird artifact
    m_fetchProgress.hide();
    m_fetchProgress.hideIfInactive(true);
    m_fetchProgress.setFixedHeight(24);
    m_fetchProgress.progressFormat("");

    m_ui->verticalLayout->insertWidget(1, &m_fetchProgress);

    auto* delegate = new ProjectItemDelegate(this);
    m_ui->packView->setItemDelegate(delegate);
    m_ui->packView->installEventFilter(this);
    m_ui->packView->viewport()->installEventFilter(this);

    connect(m_ui->packDescription, &QTextBrowser::anchorClicked, this, &ResourcePage::openUrl);

    connect(m_ui->packView, &QAbstractItemView::doubleClicked, this, &ResourcePage::onResourceToggle);
    connect(delegate, &ProjectItemDelegate::checkboxClicked, this, &ResourcePage::onResourceToggle);
}

ResourcePage::~ResourcePage()
{
    delete m_ui;
    delete m_model;
}

void ResourcePage::useCardPresentation()
{
    if (m_cardPresentation) return;
    m_cardPresentation = true;
    auto* delegate = new Pxl::CatalogCardDelegate(this,
        [this](const QModelIndex& index) { onResourceToggle(index); },
        [this](const QModelIndex& index) { showProjectDetails(index); });
    m_ui->packView->setItemDelegate(delegate);
    m_ui->packView->setMouseTracking(true);
    m_ui->packView->setSpacing(5);
    m_ui->packView->setStyleSheet("QListView { background: transparent; border: none; outline: none; }");
    m_ui->packView->viewport()->setAutoFillBackground(false);
    m_ui->packDescription->hide();
    m_ui->label->hide();
    m_ui->versionSelectionBox->hide();
    m_ui->resourceSelectionButton->hide();
    auto* sorting = new QWidget(this);
    sorting->setObjectName("catalogSortingRow");
    sorting->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto* row = new QHBoxLayout(sorting);
    row->setContentsMargins(0, 0, 0, 0);
    auto* sort = new Pxl::ChoiceChip(sorting);
    sort->setObjectName("catalogSortChoice");
    sort->setAccessibleName(tr("Sort results"));
    sort->setMaximumWidth(320);
    // Keep provider-specific handlers on the original control and share its model.
    sort->setModel(m_ui->sortByBox->model());
    sort->setCurrentIndex(m_ui->sortByBox->currentIndex());
    connect(sort, qOverload<int>(&QComboBox::currentIndexChanged), m_ui->sortByBox, &QComboBox::setCurrentIndex);
    connect(m_ui->sortByBox, qOverload<int>(&QComboBox::currentIndexChanged), sort, [sort](int index) {
        const QSignalBlocker block(sort);
        sort->setCurrentIndex(index);
    });
    m_ui->sortByBox->hide();
    row->addWidget(sort);
    row->addStretch();
    m_ui->verticalLayout->insertWidget(1, sorting);
    m_ui->verticalLayout->setContentsMargins(0, 0, 0, 0);
    m_ui->verticalLayout->setSpacing(12);
    for (int i = 0; i < m_ui->verticalLayout->count(); ++i) m_ui->verticalLayout->setStretch(i, 0);
    m_ui->verticalLayout->setStretchFactor(m_ui->splitter, 1);
    Pxl::applySearchSurface(m_ui->searchEdit);
    disconnect(m_ui->packView, &QAbstractItemView::doubleClicked, this, &ResourcePage::onResourceToggle);
    // Card body activation is owned by the delegate; avoid opening twice on double-click.
}

void ResourcePage::showProjectDetails(const QModelIndex& index)
{
    if (!index.isValid()) return;
    m_ui->packView->setCurrentIndex(index);
    const auto pack = getCurrentPack();
    if (!pack) return;
    QDialog details(this);
    details.setObjectName("pxlProjectPreview");
    const QPointer<ResourcePage> pageGuard(this);
    connect(this,&QObject::destroyed,&details,&QDialog::reject);
    details.setProperty("pxlFlow", true);
    details.setProperty("pxlPreferredSize", QSize(820, 680));
    auto* layout = new QVBoxLayout(&details);
    layout->setContentsMargins(24,24,24,24);
    layout->setSpacing(18);
    auto* header = new QHBoxLayout;
    header->setSpacing(14);
    auto* icon = new QLabel(&details);
    auto artwork = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
    if (artwork.isNull()) artwork = QIcon(":/pxl/icons/squares-four.svg");
    icon->setPixmap(artwork.pixmap(56,56));
    icon->setFixedSize(56,56);
    header->addWidget(icon);
    auto* identity = new QVBoxLayout;
    identity->setSpacing(5);
    auto* title = new QLabel(pack->name,&details);
    title->setTextFormat(Qt::PlainText);
    title->setWordWrap(true);
    title->setProperty("pxlFlowTitle",true);
    identity->addWidget(title);
    QStringList authors;
    for (const auto& author : pack->authors) authors.append(author.name);
    auto* author = new QLabel(authors.join(", "),&details);
    author->setTextFormat(Qt::PlainText);
    author->setProperty("pxlFlowHint",true);
    author->setWordWrap(true);
    identity->addWidget(author);
    header->addLayout(identity,1);
    auto* close = new QPushButton(tr("Close"),&details);
    close->setAutoDefault(false);
    connect(close,&QPushButton::clicked,&details,&QDialog::accept);
    header->addWidget(close,0,Qt::AlignTop);
    layout->addLayout(header);
    // New presentation, shared document/data pipeline: network image loading,
    // provider links and asynchronous metadata continue to use the existing engine.
    auto* description = new QTextBrowser(&details);
    description->setDocument(m_ui->packDescription->document());
    description->setOpenLinks(false);
    description->setOpenExternalLinks(false);
    description->setFrameShape(QFrame::NoFrame);
    description->setStyleSheet("QTextBrowser { background: rgba(17,21,26,110); border: 1px solid rgba(188,200,216,25); border-radius: 12px; padding: 12px; }");
    description->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(description,&QTextBrowser::anchorClicked,this,&ResourcePage::openUrl);
    layout->addWidget(description,1);
    auto* footer = new QHBoxLayout;
    footer->setSpacing(12);
    auto* versions = new Pxl::ChoiceChip(&details);
    versions->setAccessibleName(tr("Content version"));
    versions->setModel(m_ui->versionSelectionBox->model());
    versions->setCurrentIndex(m_ui->versionSelectionBox->currentIndex());
    connect(versions,qOverload<int>(&QComboBox::currentIndexChanged),m_ui->versionSelectionBox,&QComboBox::setCurrentIndex);
    connect(m_ui->versionSelectionBox,qOverload<int>(&QComboBox::currentIndexChanged),versions,[versions](int current) {
        const QSignalBlocker blocker(versions); versions->setCurrentIndex(current);
    });
    footer->addWidget(versions,1);
    auto* select = new QPushButton(&details);
    select->setObjectName("projectPreviewSelect");
    select->setProperty("pxlPrimary",true);
    select->setAutoDefault(false);
    auto updateAction = [pageGuard,select] {
        if (!pageGuard) { select->setEnabled(false); return; }
        const auto current=pageGuard->getCurrentPack();
        const int version = pageGuard->m_selectedVersionIndex;
        const auto installedId = current ? pageGuard->m_model->getInstalledPackVersion(current) : QVariant();
        const bool installed = current && version >= 0 && version < current->versions.size() && installedId.isValid()
                               && current->versions[version].fileId == installedId && !current->versions[version].is_currently_selected;
        select->setEnabled(current && current->versionsLoaded && version >= 0 && version < current->versions.size()
                           && !installed && pageGuard->m_model->checkVersionFilters(current->versions[version]));
        select->setText(installed ? tr("Installed") : current && current->isAnyVersionSelected() ? tr("Remove from selection") : tr("Add to selection"));
        select->ensurePolished();
        select->setMinimumWidth(select->fontMetrics().horizontalAdvance(select->text()) + 40);
    };
    // The overlay disables its underlying page. Calling a hidden source button
    // would silently do nothing; invoke the same checked engine handler directly.
    connect(select,&QPushButton::clicked,&details,[pageGuard,updateAction] { if (pageGuard) pageGuard->onResourceSelected(); updateAction(); });
    auto deferAction = [&details,updateAction] { QTimer::singleShot(0,&details,updateAction); };
    connect(m_ui->versionSelectionBox,qOverload<int>(&QComboBox::currentIndexChanged),&details,deferAction);
    connect(versions->model(),&QAbstractItemModel::rowsInserted,&details,deferAction);
    connect(versions->model(),&QAbstractItemModel::modelReset,&details,deferAction);
    updateAction();
    footer->addWidget(select);
    layout->addLayout(footer);
    Pxl::DialogOverlay::run(&details,window());

}

void ResourcePage::retranslate()
{
    m_ui->retranslateUi(this);
}

void ResourcePage::openedImpl()
{
    if (!supportsFiltering()) {
        m_ui->resourceFilterButton->setVisible(false);
        m_ui->filterWidget->hide();
    }

    //: String in the search bar of the mod downloading dialog
    m_ui->searchEdit->setPlaceholderText(tr("Search for %1...").arg(resourcesString()));
    m_ui->resourceSelectionButton->setText(tr("Select %1 for download").arg(resourceString()));

    updateSelectionButton();
    if (!m_suppressInitialSearch) {
        triggerSearch();
    } else {
        m_suppressInitialSearch = false;
    }
    m_ui->searchEdit->setFocus();
}

void ResourcePage::setSuppressInitialSearch(bool suppress)
{
    m_suppressInitialSearch = suppress;
}

auto ResourcePage::eventFilter(QObject* watched, QEvent* event) -> bool
{
    if (event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (watched == m_ui->searchEdit) {
            if (keyEvent->key() == Qt::Key_Return) {
                triggerSearch();
                keyEvent->accept();
                return true;
            }
            if (m_searchTimer.isActive()) {
                m_searchTimer.stop();
            }

            m_searchTimer.start(350);

        } else if (watched == m_ui->packView) {
            // stop the event from going to the confirm button
            if (keyEvent->key() == Qt::Key_Return) {
                if (m_cardPresentation) showProjectDetails(m_ui->packView->currentIndex());
                else onResourceToggle(m_ui->packView->currentIndex());
                keyEvent->accept();
                return true;
            }
        }
    } else if (watched == m_ui->packView->viewport() && event->type() == QEvent::MouseButtonPress) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);

        if (mouseEvent->button() == Qt::MiddleButton) {
            onResourceToggle(m_ui->packView->indexAt(mouseEvent->pos()));
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

QString ResourcePage::getSearchTerm() const
{
    return m_ui->searchEdit->text();
}

void ResourcePage::setSearchTerm(const QString& term)
{
    m_ui->searchEdit->setText(term);
}

void ResourcePage::addSortings()
{
    Q_ASSERT(m_model);

    auto sorts = m_model->getSortingMethods();
    std::ranges::sort(sorts, [](const auto& l, const auto& r) { return l.index < r.index; });

    for (auto&& sorting : sorts) {
        m_ui->sortByBox->addItem(sorting.readable_name, QVariant(sorting.index));
    }
}

bool ResourcePage::setCurrentPack(ModPlatform::IndexedPack::Ptr pack)
{
    QVariant v;
    v.setValue(pack);
    return m_model->setData(m_ui->packView->currentIndex(), v, Qt::UserRole);
}

ModPlatform::IndexedPack::Ptr ResourcePage::getCurrentPack() const
{
    return m_ui->packView->currentIndex().data(Qt::UserRole).value<ModPlatform::IndexedPack::Ptr>();
}

void ResourcePage::updateUi(const QModelIndex& index)
{
    if (index != m_ui->packView->currentIndex()) {
        return;
    }

    auto currentPack = getCurrentPack();
    if (!currentPack) {
        m_ui->packDescription->setHtml({});
        m_ui->packDescription->flush();
        return;
    }
    QString text = "";
    QString name = currentPack->name;

    if (currentPack->websiteUrl.isEmpty()) {
        text = name;
    } else {
        text = "<a href=\"" + currentPack->websiteUrl + "\">" + name + "</a>";
    }

    if (!currentPack->authors.empty()) {
        auto authorToStr = [](ModPlatform::ModpackAuthor& author) -> QString {
            if (author.url.isEmpty()) {
                return author.name;
            }
            return QString("<a href=\"%1\">%2</a>").arg(author.url, author.name);
        };
        QStringList authorStrs;
        for (auto& author : currentPack->authors) {
            authorStrs.push_back(authorToStr(author));
        }
        text += "<br>" + tr(" by ") + authorStrs.join(", ");
    }

    if (m_cardPresentation) text.clear(); // Identity is presented once in the PXL header.

    if (currentPack->extraDataLoaded) {
        if (currentPack->extraData.status == "archived") {
            text += "<br><br>" + tr("<b>This project has been archived. It will not receive any further updates unless the author decides "
                                    "to unarchive the project.</b>");
        }

        if (!currentPack->extraData.donate.isEmpty()) {
            text += "<br><br>" + tr("Donate information: ");
            auto donateToStr = [](ModPlatform::DonationData& donate) -> QString {
                return QString("<a href=\"%1\">%2</a>").arg(donate.url, donate.platform);
            };
            QStringList donates;
            for (auto& donate : currentPack->extraData.donate) {
                donates.append(donateToStr(donate));
            }
            text += donates.join(", ");
        }

        if (!currentPack->extraData.issuesUrl.isEmpty() || !currentPack->extraData.sourceUrl.isEmpty() ||
            !currentPack->extraData.wikiUrl.isEmpty() || !currentPack->extraData.discordUrl.isEmpty()) {
            text += "<br><br>" + tr("External links:") + "<br>";
        }

        if (!currentPack->extraData.issuesUrl.isEmpty()) {
            text += "- " + tr("Issues: <a href=%1>%1</a>").arg(currentPack->extraData.issuesUrl) + "<br>";
        }
        if (!currentPack->extraData.wikiUrl.isEmpty()) {
            text += "- " + tr("Wiki: <a href=%1>%1</a>").arg(currentPack->extraData.wikiUrl) + "<br>";
        }
        if (!currentPack->extraData.sourceUrl.isEmpty()) {
            text += "- " + tr("Source code: <a href=%1>%1</a>").arg(currentPack->extraData.sourceUrl) + "<br>";
        }
        if (!currentPack->extraData.discordUrl.isEmpty()) {
            text += "- " + tr("Discord: <a href=%1>%1</a>").arg(currentPack->extraData.discordUrl) + "<br>";
        }
    }

    if (!text.isEmpty()) text += "<hr>";

    m_ui->packDescription->setHtml(StringUtils::htmlListPatch(
        text + (currentPack->extraData.body.isEmpty() ? currentPack->description : markdownToHTML(currentPack->extraData.body))));
    m_ui->packDescription->flush();
}

void ResourcePage::updateSelectionButton()
{
    if (!isOpened || m_selectedVersionIndex < 0) {
        m_ui->resourceSelectionButton->setEnabled(false);
        return;
    }

    m_ui->resourceSelectionButton->setEnabled(true);
    if (auto currentPack = getCurrentPack(); currentPack) {
        if (currentPack->versionsLoaded && currentPack->versions.empty()) {
            m_ui->resourceSelectionButton->setEnabled(false);
            qWarning() << tr("No version available for the selected pack");
        } else if (!currentPack->isVersionSelected(m_selectedVersionIndex)) {
            m_ui->resourceSelectionButton->setText(tr("Select %1 for download").arg(resourceString()));
        } else {
            m_ui->resourceSelectionButton->setText(tr("Deselect %1 for download").arg(resourceString()));
        }
    } else {
        qWarning() << "Tried to update the selected button but there is not a pack selected";
    }
}

void ResourcePage::versionListUpdated(const QModelIndex& index)
{
    if (index == m_ui->packView->currentIndex()) {
        auto currentPack = getCurrentPack();

        m_ui->versionSelectionBox->blockSignals(true);
        m_ui->versionSelectionBox->clear();
        m_ui->versionSelectionBox->blockSignals(false);

        if (currentPack) {
            auto installedVersion = m_model->getInstalledPackVersion(currentPack);

            for (int i = 0; i < currentPack->versions.size(); i++) {
                auto& version = currentPack->versions[i];
                if (!m_model->checkVersionFilters(version)) {
                    continue;
                }

                auto versionText = version.version;
                if (version.version_type.isValid()) {
                    versionText += QString(" [%1]").arg(version.version_type.toString());
                }
                if (installedVersion.isValid() && version.fileId == installedVersion) {
                    versionText += tr(" [installed]", "Mod version select");
                }

                m_ui->versionSelectionBox->addItem(versionText, QVariant(i));
            }
        }
        if (m_ui->versionSelectionBox->count() == 0) {
            m_ui->versionSelectionBox->addItem(tr("No valid version found."), QVariant(-1));
            m_ui->resourceSelectionButton->setText(tr("Cannot select invalid version :("));
        }

        if (m_enableQueue.contains(index.row())) {
            m_enableQueue.remove(index.row());
            onResourceToggle(index);
        } else {
            updateSelectionButton();
        }
    } else if (m_enableQueue.contains(index.row())) {
        m_enableQueue.remove(index.row());
        onResourceToggle(index);
    }
}

void ResourcePage::onSelectionChanged(QModelIndex curr, [[maybe_unused]] QModelIndex prev)
{
    if (!curr.isValid()) {
        return;
    }

    auto currentPack = getCurrentPack();

    bool requestLoad = false;
    if (!currentPack || !currentPack->versionsLoaded) {
        m_ui->resourceSelectionButton->setText(tr("Loading versions..."));
        m_ui->resourceSelectionButton->setEnabled(false);

        requestLoad = true;
    } else {
        versionListUpdated(curr);
    }

    if (currentPack && !currentPack->extraDataLoaded) {
        requestLoad = true;
    }

    // we are already requesting this
    if (m_enableQueue.contains(curr.row())) {
        requestLoad = false;
    }

    if (requestLoad) {
        m_model->loadEntry(curr);
    }

    updateUi(curr);
}

void ResourcePage::onVersionSelectionChanged(int index)
{
    m_selectedVersionIndex = m_ui->versionSelectionBox->itemData(index).toInt();
    updateSelectionButton();
}

void ResourcePage::addResourceToDialog(ModPlatform::IndexedPack::Ptr pack, ModPlatform::IndexedVersion& version)
{
    m_parentDialog->addResource(std::move(pack), version);
}

void ResourcePage::removeResourceFromDialog(const QString& packName)
{
    m_parentDialog->removeResource(packName);
}

void ResourcePage::addResourceToPage(ModPlatform::IndexedPack::Ptr pack,
                                     ModPlatform::IndexedVersion& ver,
                                     ResourceFolderModel* baseModel,
                                     QString downloadReason)
{
    bool isIndexed = !APPLICATION->settings()->get("ModMetadataDisabled").toBool();
    m_model->addPack(std::move(pack), ver, baseModel, isIndexed, std::move(downloadReason));
}

void ResourcePage::modelReset()
{
    m_enableQueue.clear();
}

void ResourcePage::removeResourceFromPage(const QString& name)
{
    m_model->removePack(name);
}

void ResourcePage::onResourceSelected()
{
    if (m_selectedVersionIndex < 0) {
        return;
    }

    auto currentPack = getCurrentPack();
    if (!currentPack || !currentPack->versionsLoaded || m_selectedVersionIndex >= currentPack->versions.size()) {
        return;
    }

    auto& version = currentPack->versions[m_selectedVersionIndex];
    const auto installedId = m_model->getInstalledPackVersion(currentPack);
    if (!version.is_currently_selected && installedId.isValid() && installedId == version.fileId) return;
    Q_ASSERT(!version.downloadUrl.isNull());
    if (version.is_currently_selected) {
        removeResourceFromDialog(currentPack->name);
    } else {
        addResourceToDialog(currentPack, version);
    }

    // Save the modified pack (and prevent warning in release build)
    [[maybe_unused]] bool set = setCurrentPack(currentPack);
    Q_ASSERT(set);

    updateSelectionButton();

    /* Force redraw on the resource list when the selection changes */
    m_ui->packView->repaint();
}

void ResourcePage::onResourceToggle(const QModelIndex& index)
{
    if (!index.isValid() || !m_model) return;
    const bool isSelected = index == m_ui->packView->currentIndex();
    auto pack = m_model->data(index, Qt::UserRole).value<ModPlatform::IndexedPack::Ptr>();
    if (!pack) return;

    if (pack->versionsLoaded) {
        if (pack->isAnyVersionSelected()) {
            removeResourceFromDialog(pack->name);
        } else {
            auto version = std::find_if(pack->versions.begin(), pack->versions.end(), [this](const ModPlatform::IndexedVersion& version) {
                return m_model->checkVersionFilters(version);
            });

            if (version == pack->versions.end()) {
                auto* errorMessage = new QMessageBox(
                    QMessageBox::Warning, tr("No versions available"),
                    tr("No versions for '%1' are available.\nThe author likely blocked third-party launchers.").arg(pack->name),
                    QMessageBox::Ok, this);

                errorMessage->open();
            } else {
                addResourceToDialog(pack, *version);
            }
        }

        if (isSelected) {
            updateSelectionButton();
        }

        // force update
        QVariant variant;
        variant.setValue(pack);
        m_model->setData(index, variant, Qt::UserRole);
    } else {
        // the model is just 1 dimensional so this is fine
        m_enableQueue.insert(index.row());

        // we can't be sure that this hasn't already been requested...
        // but this does the job well enough and there's not much point preventing edgecases
        if (!isSelected) {
            m_model->loadEntry(index);
        }
    }
}

void ResourcePage::openUrl(const QUrl& url)
{
    // do not allow other url schemes for security reasons
    if (!(url.scheme() == "http" || url.scheme() == "https")) {
        qWarning() << "Unsupported scheme" << url.scheme();
        return;
    }

    // detect URLs and search instead

    const QString address = url.host() + url.path();
    QRegularExpressionMatch match;
    QString page;

    auto handlers = urlHandlers();
    for (auto it = handlers.constKeyValueBegin(); it != handlers.constKeyValueEnd(); it++) {
        auto&& [regex, candidate] = *it;
        if (match = QRegularExpression(regex).match(address); match.hasMatch()) {
            page = candidate;
            break;
        }
    }

    if (!page.isNull() && !m_doNotJumpToMod) {
        const QString slug = match.captured(1);

        // ensure the user isn't opening the same mod
        if (auto currentPack = getCurrentPack(); currentPack && slug != currentPack->slug) {
            m_parentDialog->selectPage(page);

            auto* newPage = m_parentDialog->selectedPage();

            QLineEdit* searchEdit = newPage->m_ui->searchEdit;
            auto* model = newPage->m_model;
            QListView* view = newPage->m_ui->packView;

            auto jump = [url, slug, model, view] {
                for (int row = 0; row < model->rowCount({}); row++) {
                    const QModelIndex index = model->index(row);
                    const auto pack = model->data(index, Qt::UserRole).value<ModPlatform::IndexedPack::Ptr>();

                    if (pack->slug == slug) {
                        view->setCurrentIndex(index);
                        return;
                    }
                }

                // The final fallback.
                QDesktopServices::openUrl(url);
            };

            searchEdit->setText(slug);
            newPage->triggerSearch();

            if (model->hasActiveSearchJob()) {
                connect(model->activeSearchJob().get(), &Task::finished, jump);
            } else {
                jump();
            }

            return;
        }
    }

    // open in the user's web browser
    QDesktopServices::openUrl(url);
}

void ResourcePage::openProject(const QVariant& projectID)
{
    m_ui->sortByBox->hide();
    m_ui->searchEdit->hide();
    m_ui->resourceFilterButton->hide();
    m_ui->packView->hide();
    m_ui->resourceSelectionButton->hide();
    m_doNotJumpToMod = true;

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    auto* okBtn = buttonBox->button(QDialogButtonBox::Ok);
    okBtn->setDefault(true);
    okBtn->setAutoDefault(true);
    okBtn->setText(tr("Reinstall"));
    okBtn->setShortcut(tr("Ctrl+Return"));
    okBtn->setEnabled(false);

    auto* cancelBtn = buttonBox->button(QDialogButtonBox::Cancel);
    cancelBtn->setDefault(false);
    cancelBtn->setAutoDefault(false);
    cancelBtn->setText(tr("Cancel"));

    connect(okBtn, &QPushButton::clicked, this, [this] {
        onResourceSelected();
        m_parentDialog->accept();
    });

    connect(cancelBtn, &QPushButton::clicked, m_parentDialog, &ResourceDownloadDialog::reject);
    m_ui->gridLayout_4->addWidget(buttonBox, 1, 2);

    connect(m_ui->versionSelectionBox, &QComboBox::currentIndexChanged, this,
            [this, okBtn](int index) { okBtn->setEnabled(m_ui->versionSelectionBox->itemData(index).toInt() >= 0); });

    auto jump = [this] {
        if (m_model->rowCount({}) > 0) {
            m_ui->packView->setCurrentIndex(m_model->index(0));
            return;
        }
        m_ui->packDescription->setText(tr("The resource was not found"));
    };

    m_ui->searchEdit->setText("#" + projectID.toString());
    triggerSearch();

    if (m_model->hasActiveSearchJob()) {
        connect(m_model->activeSearchJob().get(), &Task::finished, jump);
    } else {
        jump();
    }
}
}  // namespace ResourceDownload

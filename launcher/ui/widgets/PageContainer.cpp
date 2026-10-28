// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *  Copyright (c) 2022 Jamie Mansfield <jmansfield@cadixdev.org>
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

#include "PageContainer.h"
#include "ui/pxl/SettingsMode.h"
#include "ui/pxl/SettingsSearch.h"
#include "BuildConfig.h"
#include "PageContainer_p.h"
#include "ui/pxl/Animations.h"

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStackedLayout>
#include <QStyledItemDelegate>
#include <QUrl>
#include <QAbstractButton>
#include <QGroupBox>
#include <utility>

#include "settings/SettingsObject.h"

#include "ui/widgets/IconLabel.h"

#include "Application.h"
#include "DesktopServices.h"

class PageEntryFilterModel : public QSortFilterProxyModel {
   public:
    explicit PageEntryFilterModel(QObject* parent = nullptr) : QSortFilterProxyModel(parent) {}
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override
    {
        if (index.isValid() && role == Qt::DecorationRole && property("pxlIcons").toBool()) {
            auto* model = static_cast<PageModel*>(sourceModel());
            const auto id = model->pages().at(mapToSource(index).row())->id();
            static const QHash<QString, QString> icons{
                {"launcher-settings", "gear-six"}, {"language-settings", "sliders-horizontal"},
                {"appearance-settings", "sliders-horizontal"}, {"minecraft-settings", "squares-four"},
                {"java-settings", "sliders-horizontal"}, {"accounts", "user-circle"},
                {"apis", "share-network"}, {"external-tools", "sliders-horizontal"}, {"proxy-settings", "share-network"}};
            if (icons.contains(id)) return QIcon(":/pxl/icons/" + icons.value(id) + ".svg");
        }
        return QSortFilterProxyModel::data(index, role);
    }

   protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override
    {
        const QString pattern = filterRegularExpression().pattern();
        auto* const model = static_cast<PageModel*>(sourceModel());
        auto* const page = model->pages().at(sourceRow);
        if (page->id() == property("pxlExcludedPage").toString()) return false;
        if (property("pxlBasicSettings").toBool() && QStringList{"apis", "external-tools", "proxy-settings"}.contains(page->id()))
            return false;
        if (!page->shouldDisplay()) {
            return false;
        }
        const auto query = property("pxlSearchText").toString().trimmed();
        if (!query.isEmpty()) {
            QString text = page->displayName() + " " + page->id();
            if (auto* widget = dynamic_cast<QWidget*>(page)) {
                auto searchable = [this, widget](QWidget* control) {
                    if (!property("pxlBasicSettings").toBool()) return true;
                    for (auto* ancestor = control; ancestor && ancestor != widget; ancestor = ancestor->parentWidget())
                        if (ancestor->property("pxlAdvancedSetting").toBool() || ancestor->property("pxlAdvancedTab").toBool()) return false;
                    return true;
                };
                for (auto* label : widget->findChildren<QLabel*>()) if (searchable(label)) text += " " + label->text();
                for (auto* button : widget->findChildren<QAbstractButton*>()) if (searchable(button)) text += " " + button->text();
                for (auto* group : widget->findChildren<QGroupBox*>()) if (searchable(group)) text += " " + group->title();
            }
            return Pxl::settingsSearchMatches(text,query);
        }
        // Regular contents check, then check page-filter.
        return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
    }
};

PageContainer::PageContainer(BasePageProvider* pageProvider, QString defaultId, QWidget* parent)
    : QWidget(parent)
    , m_proxyModel(new PageEntryFilterModel(this))
    , m_model(new PageModel(this))
{
    createUI();
    useSidebarStyle(true);

    int counter = 0;
    auto pages = pageProvider->getPages();
    for (auto* page : pages) {
        auto* widget = dynamic_cast<QWidget*>(page);
        widget->setParent(this);
        page->stackIndex = m_pageStack->addWidget(widget);
        page->listIndex = counter;
        page->setParentContainer(this);
        counter++;
        page->updateExtraInfo = [this](const QString& id, const QString& info) {
            if (m_currentPage && id == m_currentPage->id()) {
                m_header->setText(m_currentPage->displayName() + info);
            }
        };
    }
    m_model->setPages(pages);

    m_proxyModel->setSourceModel(m_model);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

    m_pageList->setIconSize(QSize(pageIconSize, pageIconSize));
    m_pageList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_pageList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_pageList->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    m_pageList->setModel(m_proxyModel);
    connect(m_pageList->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &PageContainer::currentChanged);
    m_pageStack->setStackingMode(QStackedLayout::StackOne);
    m_pageList->setFocus();
    selectPage(std::move(defaultId));
}

bool PageContainer::selectPage(QString pageId)
{
    // now find what we want to have selected...
    auto* page = m_model->findPageEntryById(pageId);
    QModelIndex index;
    if (page) {
        index = m_proxyModel->mapFromSource(m_model->index(page->listIndex));
    }
    if (!index.isValid()) {
        index = m_proxyModel->index(0, 0);
    }
    if (index.isValid()) {
        m_pageList->setCurrentIndex(index);
        return true;
    }
    return false;
}

BasePage* PageContainer::getPage(QString pageId)
{
    return m_model->findPageEntryById(pageId);
}

BasePage* PageContainer::selectedPage() const
{
    return m_currentPage;
}

const QList<BasePage*>& PageContainer::getPages() const
{
    return m_model->pages();
}

void PageContainer::refreshContainer()
{
    m_proxyModel->invalidate();
    if (!m_currentPage->shouldDisplay()) {
        auto index = m_proxyModel->index(0, 0);
        if (index.isValid()) {
            m_pageList->setCurrentIndex(index);
        } else {
            // FIXME: unhandled corner case: what to do when there's no page to select?
        }
    }
}

void PageContainer::createUI()
{
    m_pageStack = new QStackedLayout;
    m_pageList = new PageView;
    m_header = new QLabel();

    QFont headerLabelFont = m_header->font();
    headerLabelFont.setBold(true);
    const int pointSize = headerLabelFont.pointSize();
    if (pointSize > 0) {
        headerLabelFont.setPointSize(pointSize + 2);
    }
    m_header->setFont(headerLabelFont);

    auto* headerHLayout = new QHBoxLayout;
    const int leftMargin = QApplication::style()->pixelMetric(QStyle::PM_LayoutLeftMargin);
    headerHLayout->addSpacerItem(new QSpacerItem(leftMargin, 0, QSizePolicy::Fixed, QSizePolicy::Ignored));
    headerHLayout->addWidget(m_header);
    headerHLayout->setContentsMargins(0, 6, 0, 0);

    m_pageStack->setContentsMargins(0, 0, 0, 0);
    m_pageStack->addWidget(new QWidget(this));

    m_layout = new QGridLayout;
    m_layout->addLayout(headerHLayout, 0, 1, 1, 1);
    m_layout->addWidget(m_pageList, 0, 0, 3, 1);
    m_layout->addLayout(m_pageStack, 1, 1, 1, 1);
    m_layout->setColumnStretch(1, 4);
    m_layout->setContentsMargins(0, 0, 0, 0);
    setLayout(m_layout);
}

void PageContainer::retranslate()
{
    if (m_currentPage) {
        m_header->setText(m_currentPage->displayName());
    }

    for (auto* page : m_model->pages()) {
        page->retranslate();
    }
}

void PageContainer::addButtons(QWidget* buttons)
{
    m_layout->addWidget(buttons, property("pxlTopControls").toBool() ? 3 : 2, 1, 1, 2);
}

void PageContainer::addButtons(QLayout* buttons)
{
    m_layout->addLayout(buttons, property("pxlTopControls").toBool() ? 3 : 2, 1, 1, 2);
}

void PageContainer::setTopControls(QWidget* controls)
{
    if (!controls || property("pxlTopControls").toBool()) return;
    auto* heading = m_layout->itemAtPosition(0,1);
    auto* page = m_layout->itemAtPosition(1,1);
    auto* navigation = m_layout->itemAtPosition(0,0);
    auto* buttons = m_layout->itemAtPosition(2,1);
    if (!heading || !page || !navigation) return;
    // Global settings pages share a single content edge, including their headings.
    if (auto* headingLayout = heading->layout()) {
        headingLayout->setContentsMargins(0,0,0,0);
        if (headingLayout->count() && headingLayout->itemAt(0)->spacerItem()) delete headingLayout->takeAt(0);
    }
    for (auto* entry : m_model->pages())
        if (auto* widget = dynamic_cast<QWidget*>(entry); widget && widget->layout()) widget->layout()->setContentsMargins(0,0,0,0);
    auto* empty = m_pageStack->widget(0);
    empty->setObjectName("pxlSettingsSearchEmpty");
    auto* emptyLayout = new QVBoxLayout(empty);
    emptyLayout->setContentsMargins(24,24,24,24);
    emptyLayout->setSpacing(14);
    emptyLayout->addStretch();
    auto* icon = new QLabel(empty);
    icon->setPixmap(QIcon(":/pxl/icons/magnifying-glass.svg").pixmap(40,40));
    icon->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(icon);
    auto* title = new QLabel(tr("No matching settings"),empty);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size:20px; font-weight:600; background:transparent;");
    emptyLayout->addWidget(title);
    auto* hint = new QLabel(tr("Try another word or clear the search."),empty);
    hint->setWordWrap(true); hint->setAlignment(Qt::AlignCenter);
    hint->setProperty("pxlFlowHint",true);
    emptyLayout->addWidget(hint);
    emptyLayout->addStretch(2);
    m_header->setVisible(!m_currentPage || m_currentPage->id() != "about");
    // Move the existing layout items without replacing their widgets or owners.
    for (auto* item : {heading,page,navigation,buttons}) if (item) m_layout->removeItem(item);
    m_layout->addItem(heading,1,1);
    m_layout->addItem(page,2,1);
    m_layout->addItem(navigation,0,0,4,1,Qt::AlignTop);
    if (buttons) m_layout->addItem(buttons,3,1,1,2);
    m_layout->addWidget(controls,0,1);
    setProperty("pxlTopControls",true);
}

void PageContainer::useSidebarStyle(bool sidebar)
{
    m_pageList->setProperty("_kde_side_panel_view", sidebar);
}

void PageContainer::useConeStyle()
{
    setProperty("pxlFlow", true);
    m_proxyModel->setProperty("pxlIcons", true);
    useSidebarStyle(false);
    m_pageList->setObjectName("conePageNavigation");
    m_pageList->setProperty("pxlCompactNavigation",true);
    m_pageList->setSizePolicy(QSizePolicy::MinimumExpanding,QSizePolicy::Preferred);
    m_layout->setAlignment(m_pageList,Qt::AlignTop);
    connect(m_proxyModel,&QAbstractItemModel::rowsInserted,m_pageList,&QWidget::updateGeometry);
    connect(m_proxyModel,&QAbstractItemModel::rowsRemoved,m_pageList,&QWidget::updateGeometry);
    connect(m_proxyModel,&QAbstractItemModel::modelReset,m_pageList,&QWidget::updateGeometry);
    connect(m_proxyModel,&QAbstractItemModel::layoutChanged,m_pageList,&QWidget::updateGeometry);
    m_pageList->setIconSize(QSize(22, 22));
    m_pageList->setMinimumWidth(180);
    m_pageList->setMaximumWidth(230);
    m_pageList->setFrameShape(QFrame::NoFrame);
    m_header->setObjectName("conePageHeading");
    m_layout->setHorizontalSpacing(20);
    m_layout->setVerticalSpacing(16);
}

void PageContainer::setExcludedPage(const QString& id)
{
    m_proxyModel->setProperty("pxlExcludedPage", id);
    m_proxyModel->invalidate();
}

void PageContainer::setAdvancedMode(bool advanced)
{
    const bool selectedAdvanced = m_currentPage && QStringList{"apis", "external-tools", "proxy-settings"}.contains(m_currentPage->id());
    Pxl::applySettingsMode(this, advanced);
    m_proxyModel->setProperty("pxlBasicSettings", !advanced);
    m_proxyModel->invalidate();
    if (!advanced && selectedAdvanced) selectPage("launcher-settings");
    if (m_proxyModel->rowCount() > 0 && !m_pageList->currentIndex().isValid())
        m_pageList->setCurrentIndex(m_proxyModel->index(0, 0));
}

void PageContainer::setPageHeadingVisible(bool visible)
{
    m_header->setVisible(visible);
}

void PageContainer::setSearchText(const QString& text)
{
    m_proxyModel->setProperty("pxlSearchText", text);
    m_proxyModel->invalidate();
    if (m_proxyModel->rowCount() == 0) {
        m_pageList->setCurrentIndex({});
        showPage(-1);
        return;
    }
    auto selected = m_currentPage ? m_proxyModel->mapFromSource(m_model->index(m_currentPage->listIndex)) : QModelIndex();
    if (!selected.isValid()) selected = m_proxyModel->index(0,0);
    m_pageList->setCurrentIndex(selected);
    auto* page = m_model->pages().at(m_proxyModel->mapToSource(selected).row());
    if (page != m_currentPage) showPage(page->listIndex);
}

void PageContainer::showPage(int row)
{
    if (m_currentPage) {
        m_currentPage->closed();
        if (auto* widget = dynamic_cast<QWidget*>(m_currentPage)) widget->hide();
    }
    if (row != -1) {
        m_currentPage = m_model->pages().at(row);
    } else {
        m_currentPage = nullptr;
    }
    if (m_currentPage) {
        m_pageStack->setCurrentIndex(m_currentPage->stackIndex);
        m_header->setText(m_currentPage->displayName());
        m_header->setVisible(!property("pxlTopControls").toBool() || m_currentPage->id() != "about");
        m_currentPage->opened();
        if (auto* widget = dynamic_cast<QWidget*>(m_currentPage)) {
            widget->show();
            widget->update();
            if (m_pageList->property("pxlCompactNavigation").toBool()) Pxl::animatePage(widget);
        }
    } else {
        m_pageStack->setCurrentIndex(0);
        m_header->setText(QString());
        if (property("pxlTopControls").toBool()) m_header->hide();
    }
}

void PageContainer::help()
{
    if (m_currentPage) {
        QString pageId = m_currentPage->helpPage();
        if (pageId.isEmpty()) {
            return;
        }
        DesktopServices::openUrl(QUrl(BuildConfig.HELP_URL.arg(pageId)));
    }
}

void PageContainer::currentChanged(const QModelIndex& current)
{
    int selectedIndex = current.isValid() ? m_proxyModel->mapToSource(current).row() : -1;

    if (selectedIndex < 0 || selectedIndex >= m_model->pages().size()) {
        showPage(-1);
        return;
    }

    auto* selected = m_model->pages().at(selectedIndex);
    auto* previous = m_currentPage;

    emit selectedPageChanged(previous, selected);

    showPage(selectedIndex);
}

bool PageContainer::prepareToClose()
{
    if (!saveAll()) {
        return false;
    }
    if (m_currentPage) {
        m_currentPage->closed();
    }
    return true;
}

bool PageContainer::saveAll()
{
    for (auto* page : m_model->pages()) {
        if (!page->apply()) {
            return false;
        }
    }
    return true;
}

void PageContainer::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslate();
    }
    QWidget::changeEvent(event);
}

// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2022 Jamie Mansfield <jmansfield@cadixdev.org>
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

#include "CustomPage.h"
#include "ui_CustomPage.h"

#include <QTabBar>

#include "Application.h"
#include "Filter.h"
#include "Version.h"
#include "meta/Index.h"
#include "meta/VersionList.h"
#include "minecraft/VanillaInstanceCreationTask.h"
#include "ui/dialogs/NewInstanceDialog.h"
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QUrl>
#include <QVBoxLayout>

CustomPage::CustomPage(NewInstanceDialog* dialog, QWidget* parent) : QWidget(parent), dialog(dialog), ui(new Ui::CustomPage)
{
    ui->setupUi(this);
    ui->versionList->useCompactPresentation();
    ui->loaderVersionList->useCompactPresentation();
    ui->verticalLayout_5->removeWidget(ui->scrollArea);
    ui->scrollArea->hide();
    auto* form = new QWidget(this);
    auto* fields = new QVBoxLayout(form);
    fields->setContentsMargins(0, 0, 0, 0);
    fields->setSpacing(16);
    auto label = [form, fields](const QString& text) {
        auto* result = new QLabel(text, form);
        result->setStyleSheet("font-weight: 600;");
        fields->addWidget(result);
        return result;
    };
    label(tr("Minecraft version"));
    auto* minecraft = new QHBoxLayout;
    minecraft->addWidget(ui->versionList, 1);
    minecraft->addWidget(ui->refreshBtn);
    fields->addLayout(minecraft);
    auto* filtersButton = new QPushButton(tr("Version filters"), form);
    filtersButton->setIcon(QIcon(":/pxl/icons/sliders-horizontal.svg"));
    filtersButton->setCheckable(true);
    fields->addWidget(filtersButton, 0, Qt::AlignLeft);
    auto* filterOptions = new QWidget(form);
    auto* filters = new QHBoxLayout(filterOptions);
    filters->setContentsMargins(0, 0, 0, 0);
    for (auto* filter : {ui->releaseFilter, ui->snapshotFilter, ui->betaFilter, ui->alphaFilter, ui->experimentsFilter})
        filters->addWidget(filter);
    filterOptions->hide();
    connect(filtersButton, &QPushButton::toggled, filterOptions, &QWidget::setVisible);
    fields->addWidget(filterOptions);
    auto* loaderLabel = label(tr("Loader"));
    auto* loaders = new QHBoxLayout;
    ui->noneFilter->setText(tr("Vanilla"));
    for (auto* loader : {ui->noneFilter, ui->fabricFilter, ui->forgeFilter, ui->neoForgeFilter, ui->quiltFilter, ui->liteLoaderFilter}) {
        loader->setProperty("pxlChoice", true);
        loaders->addWidget(loader);
    }
    loaders->addStretch();
    fields->addLayout(loaders);
    fields->removeWidget(loaderLabel);
    fields->removeItem(loaders);
    fields->insertWidget(0, loaderLabel);
    fields->insertLayout(1, loaders);
    auto* loaderOptions = new QWidget(form);
    auto* loaderFields = new QVBoxLayout(loaderOptions);
    loaderFields->setContentsMargins(0, 0, 0, 0);
    loaderFields->setSpacing(12);
    auto* loaderHeading = new QLabel(tr("Loader version"), loaderOptions);
    loaderHeading->setStyleSheet("font-weight: 600;");
    loaderFields->addWidget(loaderHeading);
    auto* version = new QHBoxLayout;
    version->addWidget(ui->loaderVersionList, 1);
    version->addWidget(ui->loaderRefreshBtn);
    loaderFields->addLayout(version);
    loaderOptions->setHidden(ui->noneFilter->isChecked());
    connect(ui->noneFilter, &QRadioButton::toggled, loaderOptions, &QWidget::setHidden);
    fields->addWidget(loaderOptions);
    fields->removeWidget(filtersButton);
    fields->removeWidget(filterOptions);
    fields->addWidget(filtersButton, 0, Qt::AlignLeft);
    filtersButton->setText(tr("More options"));
    filtersButton->setProperty("pxlQuiet", true);
    fields->addWidget(filterOptions);
    for (auto* refresh : {ui->refreshBtn, ui->loaderRefreshBtn}) {
        refresh->setToolTip(tr("Refresh"));
        refresh->setText(QString());
        refresh->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
        refresh->setFixedSize(36, 36);
    }
    auto* optifine = new QPushButton(tr("OptiFine installation guide"), form);
    optifine->setIcon(QIcon(":/pxl/icons/arrow-square-in.svg"));
    optifine->setToolTip(tr("Create Vanilla, then add an extracted OptiFine jar mod from the Version page. Steps depend on the OptiFine version."));
    connect(optifine, &QPushButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl("https://prismlauncher.org/wiki/getting-started/installing-optifine/"));
    });
    fields->addWidget(optifine, 0, Qt::AlignLeft);
    optifine->hide();
    connect(filtersButton, &QPushButton::toggled, optifine, &QWidget::setVisible);
    fields->addStretch();
    auto* scroll = new QScrollArea(this);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(form);
    form->setAutoFillBackground(false);
    scroll->setAutoFillBackground(false);
    scroll->setStyleSheet("QScrollArea { background: transparent; border: none; } QScrollArea > QWidget > QWidget { background: transparent; }");
    scroll->viewport()->setAutoFillBackground(false);
    ui->verticalLayout_5->addWidget(scroll);
    connect(ui->versionList, &VersionSelectWidget::selectedVersionChanged, this, &CustomPage::setSelectedVersion);
    filterChanged();
    connect(ui->alphaFilter, &QCheckBox::stateChanged, this, &CustomPage::filterChanged);
    connect(ui->betaFilter, &QCheckBox::stateChanged, this, &CustomPage::filterChanged);
    connect(ui->snapshotFilter, &QCheckBox::stateChanged, this, &CustomPage::filterChanged);
    connect(ui->releaseFilter, &QCheckBox::stateChanged, this, &CustomPage::filterChanged);
    connect(ui->experimentsFilter, &QCheckBox::stateChanged, this, &CustomPage::filterChanged);
    connect(ui->refreshBtn, &QPushButton::clicked, this, &CustomPage::refresh);

    connect(ui->loaderVersionList, &VersionSelectWidget::selectedVersionChanged, this, &CustomPage::setSelectedLoaderVersion);
    connect(ui->noneFilter, &QRadioButton::toggled, this, &CustomPage::loaderFilterChanged);
    connect(ui->forgeFilter, &QRadioButton::toggled, this, &CustomPage::loaderFilterChanged);
    connect(ui->neoForgeFilter, &QRadioButton::toggled, this, &CustomPage::loaderFilterChanged);
    connect(ui->fabricFilter, &QRadioButton::toggled, this, &CustomPage::loaderFilterChanged);
    connect(ui->quiltFilter, &QRadioButton::toggled, this, &CustomPage::loaderFilterChanged);
    connect(ui->liteLoaderFilter, &QRadioButton::toggled, this, &CustomPage::loaderFilterChanged);
    connect(ui->loaderRefreshBtn, &QPushButton::clicked, this, &CustomPage::loaderRefresh);
}

void CustomPage::openedImpl()
{
    if (!initialized) {
        auto vlist = APPLICATION->metadataIndex()->get("net.minecraft");
        ui->versionList->initialize(vlist.get());
        initialized = true;
    } else {
        suggestCurrent();
    }
}

void CustomPage::refresh()
{
    ui->versionList->loadList(true);
}

void CustomPage::loaderRefresh()
{
    if (ui->noneFilter->isChecked())
        return;
    ui->loaderVersionList->loadList(true);
}

void CustomPage::filterChanged()
{
    QStringList out;
    if (ui->alphaFilter->isChecked())
        out << "(alpha)";
    if (ui->betaFilter->isChecked())
        out << "(beta)";
    if (ui->snapshotFilter->isChecked())
        out << "(snapshot)";
    if (ui->releaseFilter->isChecked())
        out << "(release)";
    if (ui->experimentsFilter->isChecked())
        out << "(experiment)";
    auto regexp = out.join('|');
    ui->versionList->setFilter(BaseVersionList::TypeRole, Filters::regexp(QRegularExpression(regexp)));
}

void CustomPage::loaderFilterChanged()
{
    m_selectedLoaderVersion.reset();
    ui->loaderVersionList->setEnabled(!ui->noneFilter->isChecked());
    ui->loaderRefreshBtn->setEnabled(!ui->noneFilter->isChecked());
    QString minecraftVersion;
    if (m_selectedVersion) {
        minecraftVersion = m_selectedVersion->descriptor();
    } else {
        ui->loaderVersionList->setExactFilter(BaseVersionList::ParentVersionRole, "AAA");  // empty list
        ui->loaderVersionList->setEmptyString(tr("No Minecraft version is selected."));
        ui->loaderVersionList->setEmptyMode(VersionListView::String);
        return;
    }
    if (ui->noneFilter->isChecked()) {
        m_selectedLoader.clear();
        ui->loaderVersionList->setExactFilter(BaseVersionList::ParentVersionRole, "AAA");  // empty list
        ui->loaderVersionList->setEmptyString(tr("No mod loader is selected."));
        ui->loaderVersionList->setEmptyMode(VersionListView::String);
        suggestCurrent();
        return;
    } else if (ui->neoForgeFilter->isChecked()) {
        ui->loaderVersionList->setExactFilter(BaseVersionList::ParentVersionRole, minecraftVersion);
        m_selectedLoader = "net.neoforged";
    } else if (ui->forgeFilter->isChecked()) {
        ui->loaderVersionList->setExactFilter(BaseVersionList::ParentVersionRole, minecraftVersion);
        m_selectedLoader = "net.minecraftforge";
    } else if (ui->fabricFilter->isChecked()) {
        // FIXME: dirty hack because the launcher is unaware of Fabric's dependencies
        if (Version(minecraftVersion) >= Version("1.14"))  // Fabric/Quilt supported
            ui->loaderVersionList->setExactFilter(BaseVersionList::ParentVersionRole, "");
        else                                                                                   // Fabric/Quilt unsupported
            ui->loaderVersionList->setExactFilter(BaseVersionList::ParentVersionRole, "AAA");  // clear list
        m_selectedLoader = "net.fabricmc.fabric-loader";
    } else if (ui->quiltFilter->isChecked()) {
        // FIXME: dirty hack because the launcher is unaware of Quilt's dependencies (same as Fabric)
        if (Version(minecraftVersion) >= Version("1.14"))  // Fabric/Quilt supported
            ui->loaderVersionList->setExactFilter(BaseVersionList::ParentVersionRole, "");
        else                                                                                   // Fabric/Quilt unsupported
            ui->loaderVersionList->setExactFilter(BaseVersionList::ParentVersionRole, "AAA");  // clear list
        m_selectedLoader = "org.quiltmc.quilt-loader";
    } else if (ui->liteLoaderFilter->isChecked()) {
        ui->loaderVersionList->setExactFilter(BaseVersionList::ParentVersionRole, minecraftVersion);
        m_selectedLoader = "com.mumfrey.liteloader";
    } else {
        dialog->setSuggestedPack();
        return;
    }

    auto vlist = APPLICATION->metadataIndex()->get(m_selectedLoader);
    ui->loaderVersionList->initialize(vlist.get());
    ui->loaderVersionList->selectRecommended();
    // A preserved recommendation may keep the same index and emit no change
    // signal, even though changing Minecraft/loader cleared our selected value.
    setSelectedLoaderVersion(ui->loaderVersionList->selectedVersion());
    ui->loaderVersionList->setEmptyString(tr("No versions are currently available for Minecraft %1").arg(minecraftVersion));
}

CustomPage::~CustomPage()
{
    delete ui;
}

bool CustomPage::shouldDisplay() const
{
    return true;
}

void CustomPage::retranslate()
{
    ui->retranslateUi(this);
}

BaseVersion::Ptr CustomPage::selectedVersion() const
{
    return m_selectedVersion;
}

BaseVersion::Ptr CustomPage::selectedLoaderVersion() const
{
    return m_selectedLoaderVersion;
}

QString CustomPage::selectedLoader() const
{
    return m_selectedLoader;
}

void CustomPage::suggestCurrent()
{
    if (!isOpened) {
        return;
    }

    if (!m_selectedVersion) {
        dialog->setSuggestedPack();
        return;
    }

    // There isn't a selected version if the version list is empty
    if (ui->noneFilter->isChecked())
        dialog->setSuggestedPack(m_selectedVersion->descriptor(), new VanillaCreationTask(m_selectedVersion));
    else if (!m_selectedLoaderVersion) {
        dialog->setSuggestedPack();
        return;
    }
    else {
        dialog->setSuggestedPack(m_selectedVersion->descriptor(),
                                 new VanillaCreationTask(m_selectedVersion, m_selectedLoader, m_selectedLoaderVersion));
    }
    dialog->setSuggestedIcon("default");
}

void CustomPage::setSelectedVersion(BaseVersion::Ptr version)
{
    m_selectedVersion = version;
    suggestCurrent();
    loaderFilterChanged();
}

void CustomPage::setSelectedLoaderVersion(BaseVersion::Ptr version)
{
    m_selectedLoaderVersion = version;
    suggestCurrent();
}

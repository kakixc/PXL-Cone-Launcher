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

#include "NewInstanceDialog.h"
#include "Application.h"
#include "ui/pages/modplatform/ModpackProviderBasePage.h"
#include "ui/pages/modplatform/import_ftb/ImportFTBPage.h"
#include "ui_NewInstanceDialog.h"

#include <BaseVersion.h>
#include <InstanceList.h>
#include <icons/IconList.h>
#include <tasks/Task.h>

#include "IconPickerDialog.h"
#include "ProgressDialog.h"
#include "VersionSelectDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QLayout>
#include <QPushButton>
#include <QScreen>
#include <QStyle>
#include <QPainterPath>
#include <QCoreApplication>
#include <QValidator>
#include <utility>

#include "ui/pages/modplatform/CustomPage.h"
#include "ui/pages/modplatform/ImportPage.h"
#include "ui/pages/modplatform/atlauncher/AtlPage.h"
#include "ui/pages/modplatform/flame/FlamePage.h"
#include "ui/pages/modplatform/ftb/FtbPage.h"
#include "ui/pages/modplatform/legacy_ftb/Page.h"
#include "ui/pages/modplatform/modrinth/ModrinthPage.h"
#include "ui/pages/modplatform/technic/TechnicPage.h"
#include "ui/widgets/PageContainer.h"
#include "ui/pxl/Backdrop.h"
#include "ui/pxl/DialogOverlay.h"
#include "ui/pxl/Icons.h"
#include "settings/SettingsObject.h"

NewInstanceDialog::NewInstanceDialog(const QString& initialGroup,
                                     const QString& url,
                                     const QMap<QString, QString>& extra_info,
                                     QWidget* parent)
    : QDialog(parent), ui(new Ui::NewInstanceDialog)
{
    ui->setupUi(this);
    setProperty("pxlFlow", true);
    ui->verticalLayout->setContentsMargins(24, 24, 24, 24);
    ui->verticalLayout->setSpacing(16);
    auto* title = new QLabel(tr("Create an instance"), this);
    title->setProperty("pxlFlowTitle", true);
    title->setStyleSheet("font-size: 21px; font-weight: 700;");
    auto* hint = new QLabel(tr("How would you like to create your instance?"), this);
    m_hint = hint;
    hint->setProperty("pxlFlowHint", true);
    hint->setWordWrap(true);
    ui->verticalLayout->insertWidget(0, title);
    ui->verticalLayout->insertWidget(1, hint);
    ui->verticalLayout->removeItem(ui->gridLayout_2);
    m_identity = new QWidget(this);
    for (auto* field : QList<QWidget*>{ui->iconButton, ui->nameLabel, ui->instNameTextBox, ui->groupLabel, ui->groupBox})
        ui->gridLayout_2->removeWidget(field);
    delete ui->gridLayout_2;
    ui->gridLayout_2 = nullptr;
    auto* identity = new QVBoxLayout(m_identity);
    identity->setContentsMargins(0, 0, 0, 0);
    identity->setSpacing(12);
    auto* iconRow = new QHBoxLayout;
    iconRow->setSpacing(16);
    ui->iconButton->setFixedSize(88, 88);
    ui->iconButton->setIconSize(QSize(64, 64));
    ui->iconButton->setToolTip(tr("Change icon"));
    iconRow->addWidget(ui->iconButton);
    auto* nameFields = new QVBoxLayout;
    nameFields->setSpacing(8);
    ui->nameLabel->setText(tr("Name"));
    nameFields->addWidget(ui->nameLabel);
    nameFields->addWidget(ui->instNameTextBox);
    iconRow->addLayout(nameFields, 1);
    identity->addLayout(iconRow);
    auto* groupToggle = new QPushButton(tr("Group (optional)"), m_identity);
    groupToggle->setCheckable(true);
    groupToggle->setProperty("pxlQuiet", true);
    identity->addWidget(groupToggle, 0, Qt::AlignLeft);
    identity->addWidget(ui->groupBox);
    ui->groupBox->hide();
    ui->groupLabel->hide();
    connect(groupToggle, &QPushButton::toggled, ui->groupBox, &QWidget::setVisible);
    ui->verticalLayout->addWidget(m_identity);
    auto* close = new QPushButton(this);
    close->setObjectName("closeCreation");
    QPixmap closePixmap(24, 24);
    closePixmap.fill(Qt::transparent);
    {
        QPainter iconPainter(&closePixmap);
        iconPainter.setRenderHint(QPainter::Antialiasing);
        iconPainter.setPen(QPen(QColor("#b7bec8"), 2.2, Qt::SolidLine, Qt::RoundCap));
        iconPainter.drawLine(QPointF(7, 7), QPointF(17, 17));
        iconPainter.drawLine(QPointF(17, 7), QPointF(7, 17));
    }
    close->setIcon(QIcon(closePixmap));
    close->setFixedSize(36, 36);
    close->setAccessibleName(tr("Cancel"));
    close->setToolTip(tr("Cancel"));
    close->setStyleSheet("QPushButton { border-radius: 18px; padding: 0; }");
    connect(close, &QPushButton::clicked, this, &NewInstanceDialog::reject);
    auto* heading = new QHBoxLayout;
    ui->verticalLayout->removeWidget(title);
    heading->addWidget(title, 1);
    heading->addWidget(close);
    ui->verticalLayout->insertLayout(0, heading);
    ui->line->hide();

    setWindowIcon(QIcon(":/pxl/cone-icon.svg"));

    InstIconKey = "default";
    ui->iconButton->setIcon(APPLICATION->icons()->getIcon(InstIconKey));

    QStringList groups = APPLICATION->instances()->getGroups();
    groups.prepend("");
    int index = groups.indexOf(initialGroup);
    if (index == -1) {
        index = 1;
        groups.insert(index, initialGroup);
    }
    ui->groupBox->addItems(groups);
    ui->groupBox->setCurrentIndex(index);
    ui->groupBox->lineEdit()->setPlaceholderText(tr("No group"));

    // NOTE: m_buttons must be initialized before PageContainer, because it indirectly accesses m_buttons through setSuggestedPack! Do not
    // move this below.
    m_buttons = new QDialogButtonBox(QDialogButtonBox::Help | QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_create = m_buttons->button(QDialogButtonBox::Ok);

    m_container = new PageContainer(this, {}, this);
    m_container->useConeStyle();
    m_container->setSizePolicy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Expanding);
    m_container->layout()->setContentsMargins(0, 0, 0, 0);
    ui->verticalLayout->addWidget(m_container, 1);

    m_choices = new QWidget(this);
    auto* choices = new QVBoxLayout(m_choices);
    choices->setContentsMargins(0, 0, 0, 0);
    choices->setSpacing(12);
    auto addChoice = [this, choices](const QString& name, const QString& description, const QString& icon, const QString& page) {
        auto* button = new QPushButton(m_choices);
        button->setObjectName("createSource_" + page);
        button->setAccessibleName(name + ". " + description);
        button->setMinimumHeight(80);
        button->setCursor(Qt::PointingHandCursor);
        button->setStyleSheet("QPushButton { padding: 0; border-radius: 15px; border: 1px solid transparent; background: #2c3036; } QPushButton:hover { background: #353a42; } QPushButton:focus { border-color: #767b84; }");
        auto* row = new QHBoxLayout(button);
        row->setContentsMargins(14, 12, 16, 12);
        row->setSpacing(14);
        auto* artwork = new QLabel(button);
        artwork->setFixedSize(44, 44);
        artwork->setAlignment(Qt::AlignCenter);
        artwork->setPixmap(QIcon(":/pxl/icons/" + icon + ".svg").pixmap(25, 25));
        artwork->setStyleSheet("background: rgba(255,255,255,5); border: 1px solid rgba(255,255,255,15); border-radius: 11px;");
        row->addWidget(artwork);
        auto* copy = new QVBoxLayout;
        copy->setSpacing(5);
        auto* primary = new QLabel(name, button);
        primary->setStyleSheet("background: transparent; font-size: 15px; font-weight: 600; color: #f0f1f3;");
        auto* secondary = new QLabel(description, button);
        secondary->setStyleSheet("background: transparent; font-size: 13px; font-weight: 400; color: #abb3bf;");
        secondary->setWordWrap(true);
        copy->addWidget(primary);
        copy->addWidget(secondary);
        row->addLayout(copy, 1);
        for (auto* label : {artwork, primary, secondary}) label->setAttribute(Qt::WA_TransparentForMouseEvents);
        choices->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, page] { chooseSource(page); });
    };
    addChoice(tr("From scratch"), tr("Choose Minecraft and a loader"), "plus", "vanilla");
    addChoice(tr("Browse modpacks"), tr("Modrinth, CurseForge and other catalogs"), "squares-four", "modrinth");
    addChoice(tr("Import a pack file"), tr("Install a local archive or a pack URL"), "import", "import");
    addChoice(tr("Import from another launcher"), tr("Bring your existing instances and worlds"), "download-simple", "launcher");
    ui->verticalLayout->insertWidget(2, m_choices);

    m_buttons->hide();
    auto* footer = new QWidget(this);
    footer->setObjectName("creationFooter");
    auto* footerRow = new QHBoxLayout(footer);
    footerRow->setContentsMargins(0, 8, 0, 0);
    m_back = new QPushButton(tr("Back"), footer);
    m_back->setIcon(QIcon(":/pxl/icons/arrow-left.svg"));
    connect(m_back, &QPushButton::clicked, this, &NewInstanceDialog::showSourceChoices);
    footerRow->addWidget(m_back);
    footerRow->addStretch();
    connect(m_container, &PageContainer::selectedPageChanged, this, [this](BasePage* previous, BasePage* selected) {
        m_create->setEnabled(creationTask && !instName().isEmpty());
    });

    // Bonk Qt over its stupid head and make sure it understands which button is the default one...
    // See: https://stackoverflow.com/questions/24556831/qbuttonbox-set-default-button
    auto OkButton = m_create;
    m_buttons->removeButton(OkButton);
    footerRow->addWidget(OkButton);
    ui->verticalLayout->addWidget(footer);
    OkButton->setDefault(true);
    OkButton->setAutoDefault(true);
    OkButton->setText(tr("Create instance"));
    OkButton->setProperty("pxlPrimary", true);
    OkButton->setIcon(Pxl::primaryIcon("plus"));
    OkButton->style()->unpolish(OkButton);
    OkButton->style()->polish(OkButton);
    connect(OkButton, &QPushButton::clicked, this, &NewInstanceDialog::accept);

    auto CancelButton = m_buttons->button(QDialogButtonBox::Cancel);
    CancelButton->setDefault(false);
    CancelButton->setAutoDefault(false);
    CancelButton->setText(tr("Cancel"));
    connect(CancelButton, &QPushButton::clicked, this, &NewInstanceDialog::reject);

    auto HelpButton = m_buttons->button(QDialogButtonBox::Help);
    HelpButton->setDefault(false);
    HelpButton->setAutoDefault(false);
    HelpButton->setText(tr("Help"));
    connect(HelpButton, &QPushButton::clicked, m_container, &PageContainer::help);

    if (!url.isEmpty()) {
        chooseSource("import");
        QUrl actualUrl(url);
        m_container->selectPage("import");
        importPage->setUrl(url);
        importPage->setExtraInfo(extra_info);
    } else showSourceChoices();

    updateDialogState();

    const bool restored = restoreGeometry(QByteArray::fromBase64(APPLICATION->settings()->get("NewInstanceGeometry").toString().toUtf8()));
    const auto available = screen()->availableGeometry().size() - QSize(48, 80);
    const int targetHeight = qMin(860, available.height());
    resize(qMin(restored ? qMax(width(), 780) : 960, available.width()), qMin(qMax(height(), targetHeight), available.height()));

    connect(m_container, &PageContainer::selectedPageChanged, this, &NewInstanceDialog::selectedPageChanged);
}

void NewInstanceDialog::reject()
{
    APPLICATION->settings()->set("NewInstanceGeometry", QString::fromUtf8(saveGeometry().toBase64()));

    // This is just so that the pages get the close() call and can react to it, if needed.
    m_container->prepareToClose();

    QDialog::reject();
}

void NewInstanceDialog::accept()
{
    APPLICATION->settings()->set("NewInstanceGeometry", QString::fromUtf8(saveGeometry().toBase64()));
    importIconNow();

    // This is just so that the pages get the close() call and can react to it, if needed.
    m_container->prepareToClose();

    QDialog::accept();
}

QList<BasePage*> NewInstanceDialog::getPages()
{
    QList<BasePage*> pages;

    importPage = new ImportPage(this);

    pages.append(new CustomPage(this));
    pages.append(importPage);
    pages.append(new AtlPage(this));
    if (APPLICATION->capabilities() & Application::SupportsFlame)
        pages.append(new FlamePage(this));
    pages.append(new FtbPage(this));
    pages.append(new LegacyFTB::Page(this));
    pages.append(new FTBImportAPP::ImportFTBPage(this));
    pages.append(new ModrinthPage(this));
    pages.append(new TechnicPage(this));

    return pages;
}

QString NewInstanceDialog::dialogTitle()
{
    return tr("New Instance");
}

NewInstanceDialog::~NewInstanceDialog()
{
    delete ui;
}

int NewInstanceDialog::execInParent(QWidget* surface)
{
    m_overlay = true;
    layout()->setSizeConstraint(QLayout::SetNoConstraint);
    setStyleSheet("NewInstanceDialog { background: transparent; }");
    return Pxl::DialogOverlay::run(this, surface);
}

void NewInstanceDialog::showSourceChoices()
{
    m_container->hide();
    m_identity->hide();
    m_back->hide();
    m_buttons->hide();
    findChild<QWidget*>("creationFooter")->hide();
    m_choices->show();
    m_hint->show();
    m_hint->setText(tr("How would you like to create your instance?"));
    setProperty("pxlPreferredSize", QSize(580, 540));
    QCoreApplication::postEvent(this, new QEvent(QEvent::LayoutRequest));
    m_choices->findChild<QPushButton*>()->setFocus();
    update();
}

void NewInstanceDialog::chooseSource(const QString& page)
{
    if (page == "launcher") {
        m_container->prepareToClose();
        done(2);
        return;
    }
    m_choices->hide();
    m_identity->show();
    m_container->show();
    m_buttons->hide();
    findChild<QWidget*>("creationFooter")->show();
    m_back->show();
    const bool scratch = page == "vanilla";
    const bool catalog = page == "modrinth";
    m_hint->setVisible(!scratch);
    m_hint->setText(scratch ? tr("Choose a Minecraft version and loader.") :
                            catalog ? tr("Choose a catalog and find your modpack.") : tr("Choose an archive or paste a pack URL."));
    m_container->setExcludedPage(QString());
    m_container->setPageListVisible(catalog);
    m_container->setPageHeadingVisible(catalog);
    if (!m_container->selectedPage() || m_container->selectedPage()->id() != page) {
        setSuggestedPack();
        m_container->selectPage(page);
    }
    setProperty("pxlPreferredSize", catalog ? QSize(1000, 740) : scratch ? QSize(640, 650) : QSize(720, 650));
    QCoreApplication::postEvent(this, new QEvent(QEvent::LayoutRequest));
    ui->instNameTextBox->setFocus();
    update();
}

void NewInstanceDialog::paintEvent(QPaintEvent* event)
{
    QDialog::paintEvent(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath shape;
    shape.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 16, 16);
    painter.setClipPath(shape);
    painter.fillPath(shape, QColor("#202328"));
    if (!m_overlay && APPLICATION->settings()->get("WorkspaceBackground").toString() != "solid")
        Pxl::paintBackdrop(painter, rect(), Pxl::blurredBackdrop(), 215);
    painter.setPen(QColor(255, 238, 218, 35));
    painter.drawPath(shape);
}

void NewInstanceDialog::setSuggestedPack(const QString& name, InstanceTask* task)
{
    creationTask.reset(task);

    ui->instNameTextBox->setPlaceholderText(name);
    importVersion.clear();

    if (!task) {
        ui->iconButton->setIcon(APPLICATION->icons()->getIcon(InstIconKey));
        importIcon = false;
    }

    auto allowOK = task && !instName().isEmpty();
    m_create->setEnabled(allowOK);
}

void NewInstanceDialog::setSuggestedPack(const QString& name, QString version, InstanceTask* task)
{
    creationTask.reset(task);

    ui->instNameTextBox->setPlaceholderText(name);
    importVersion = std::move(version);

    if (!task) {
        ui->iconButton->setIcon(APPLICATION->icons()->getIcon(InstIconKey));
        importIcon = false;
    }

    auto allowOK = task && !instName().isEmpty();
    m_create->setEnabled(allowOK);
}

void NewInstanceDialog::setSuggestedIconFromFile(const QString& path, const QString& name)
{
    importIcon = true;
    importIconPath = path;
    importIconName = name;

    // Hmm, for some reason they can be to small
    ui->iconButton->setIcon(QIcon(path));
}

void NewInstanceDialog::setSuggestedIcon(const QString& key)
{
    if (key == "default")
        return;

    auto icon = APPLICATION->icons()->getIcon(key);
    importIcon = false;

    ui->iconButton->setIcon(icon);
}

InstanceTask* NewInstanceDialog::extractTask()
{
    InstanceTask* extracted = creationTask.release();

    InstanceName inst_name(ui->instNameTextBox->placeholderText().trimmed(), importVersion);
    inst_name.setName(ui->instNameTextBox->text().trimmed());
    extracted->setName(inst_name);

    extracted->setGroup(instGroup());
    extracted->setIcon(iconKey());
    extracted->setProperty("pxlTaskIcon", APPLICATION->icons()->getIcon(iconKey()));
    return extracted;
}

void NewInstanceDialog::updateDialogState()
{
    auto allowOK = creationTask && !instName().isEmpty();
    auto OkButton = m_create;
    if (OkButton->isEnabled() != allowOK) {
        OkButton->setEnabled(allowOK);
    }
}

QString NewInstanceDialog::instName() const
{
    auto result = ui->instNameTextBox->text().trimmed();
    if (result.size()) {
        return result;
    }
    result = ui->instNameTextBox->placeholderText().trimmed();
    if (result.size()) {
        return result;
    }
    return QString();
}

QString NewInstanceDialog::instGroup() const
{
    return ui->groupBox->currentText();
}
QString NewInstanceDialog::iconKey() const
{
    return InstIconKey;
}

void NewInstanceDialog::on_iconButton_clicked()
{
    importIconNow();  // so the user can switch back
    IconPickerDialog dlg(this);
    dlg.execWithSelection(InstIconKey);

    if (dlg.result() == QDialog::Accepted) {
        InstIconKey = dlg.selectedIconKey;
        ui->iconButton->setIcon(APPLICATION->icons()->getIcon(InstIconKey));
        importIcon = false;
    }
}

void NewInstanceDialog::on_instNameTextBox_textChanged([[maybe_unused]] const QString& arg1)
{
    updateDialogState();
}

void NewInstanceDialog::importIconNow()
{
    if (importIcon) {
        APPLICATION->icons()->installIcon(importIconPath, importIconName);
        InstIconKey = importIconName.mid(0, importIconName.lastIndexOf('.'));
        importIcon = false;
    }
    APPLICATION->settings()->set("NewInstanceGeometry", QString::fromUtf8(saveGeometry().toBase64()));
}

void NewInstanceDialog::selectedPageChanged(BasePage* previous, BasePage* selected)
{
    auto prevPage = dynamic_cast<ModpackProviderBasePage*>(previous);
    if (prevPage) {
        m_searchTerm = prevPage->getSerachTerm();
    }

    auto nextPage = dynamic_cast<ModpackProviderBasePage*>(selected);
    if (nextPage) {
        nextPage->setSearchTerm(m_searchTerm);
    }
}

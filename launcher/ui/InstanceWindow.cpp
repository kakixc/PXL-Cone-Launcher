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

#include "InstanceWindow.h"
#include "Application.h"

#include <QCloseEvent>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QAction>
#include <QStyle>
#include <QScrollBar>
#include <QStackedWidget>
#include "ui/pxl/ContentBrowser.h"
#include "ui/pxl/LibraryControls.h"
#include "ui/pxl/ActionMenu.h"
#include "ui/pxl/Icons.h"
#include "ui/pxl/Animations.h"
#include <QSignalBlocker>

#include "ui/widgets/PageContainer.h"

#include "InstancePageProvider.h"

#include "icons/IconList.h"
#include "ui/pages/instance/ModFolderPage.h"
#include "ui/pages/instance/ResourcePackPage.h"
#include "ui/pages/instance/TexturePackPage.h"
#include "ui/pages/instance/ShaderPackPage.h"
#include "ui/pages/instance/VersionPage.h"
#include "ui/pages/instance/WorldListPage.h"
#include "ui/pages/instance/LogPage.h"

InstanceWindow::InstanceWindow(BaseInstance* instance, QWidget* parent, bool embedded)
    : QMainWindow(parent), m_instance(instance), m_embedded(embedded)
{
    setAttribute(Qt::WA_DeleteOnClose);
    if (m_embedded) {
        setWindowFlags(Qt::Widget);
        setProperty("pxlFlow", true);
        setStyleSheet("InstanceWindow { background: transparent; }");
    }

    auto icon = APPLICATION->icons()->getIcon(m_instance->iconKey());
    QString windowTitle = tr("Console window for ") + m_instance->name();

    // Set window properties
    {
        setWindowIcon(icon);
        setWindowTitle(windowTitle);
    }

    // Add page container
    {
        auto provider = std::make_shared<InstancePageProvider>(m_instance);
        m_container = new PageContainer(provider.get(), m_embedded ? "mods" : "console", this);
        m_container->useConeStyle();
        m_container->setParentContainer(this);
        setCentralWidget(m_container);
        setContentsMargins(0, 0, 0, 0);
    }

    if (m_embedded) {
        auto* body = new QWidget(this);
        auto* layout = new QVBoxLayout(body);
        layout->setContentsMargins(24, 16, 24, 24);
        layout->setSpacing(16);
        auto* tabs = new QHBoxLayout;
        if (auto* versions = dynamic_cast<VersionPage*>(m_container->getPage("version"))) versions->usePxlPresentation();
        if (auto* worlds = dynamic_cast<WorldListPage*>(m_container->getPage("worlds"))) worlds->usePxlPresentation();
        if (auto* logs = dynamic_cast<LogPage*>(m_container->getPage("console"))) logs->usePxlPresentation();
        if (dynamic_cast<MinecraftInstance*>(m_instance))
        for (const auto& id : {QStringLiteral("mods"),QStringLiteral("resourcepacks"),QStringLiteral("texturepacks"),QStringLiteral("shaderpacks")})
            if (auto* page=dynamic_cast<ExternalResourcesPage*>(m_container->getPage(id)))
                page->usePxlPresentation([this,id] { browseContent(id=="texturepacks" ? "resourcepacks" : id); });
        const QString resourcePage = m_instance->traits().contains("texturepacks") ? "texturepacks" : "resourcepacks";
        const QList<QPair<QString, QString>> categories{{"mods", tr("Mods")}, {resourcePage, tr("Resource packs")},
            {"shaderpacks", tr("Shaders")}, {"worlds", tr("Worlds")}, {"console", tr("Logs")}, {"version", tr("Advanced")}};
        for (const auto& category : categories) {
            auto* button = new QPushButton(category.second, body);
            button->setCheckable(true);
            button->setProperty("pxlMode", true);
            button->setProperty("pxlPage", category.first);
            tabs->addWidget(button);
            m_editorTabs.append(button);
            connect(button, &QPushButton::clicked, this, [this, id = category.first] { selectPage(id); });
        }
        tabs->addStretch();
        layout->addLayout(tabs);
        m_advancedChoice = new Pxl::ChoiceChip(body);
        m_advancedChoice->setObjectName("instanceAdvancedSection");
        m_advancedChoice->setMinimumWidth(260);
        for (auto* page : m_container->getPages()) {
            if (QStringList{"mods", "resourcepacks", "texturepacks", "shaderpacks", "worlds", "console"}.contains(page->id())) continue;
            if (page->shouldDisplay()) m_advancedChoice->addItem(page->id() == "version" ? tr("Minecraft components") : page->displayName(), page->id());
        }
        connect(m_advancedChoice, qOverload<int>(&QComboBox::activated), this, [this](int row) {
            selectPage(m_advancedChoice->itemData(row).toString());
        });
        layout->addWidget(m_advancedChoice, 0, Qt::AlignLeft);
        auto* actions = new QHBoxLayout;
        m_browseContent = new QPushButton(tr("Browse content"), body);
        m_browseContent->setIcon(QIcon(":/pxl/icons/download-simple.svg"));
        m_addContent = new QPushButton(tr("Add files"), body);
        m_addContent->setIcon(QIcon(":/pxl/icons/plus.svg"));
        actions->addWidget(m_browseContent);
        actions->addWidget(m_addContent);
        actions->addStretch();
        layout->addLayout(actions);
        connect(m_browseContent, &QPushButton::clicked, this, [this] {
            QString category = selectedPage() ? selectedPage()->id() : QString();
            if (category == "texturepacks") category = "resourcepacks";
            browseContent(category);
        });
        connect(m_addContent, &QPushButton::clicked, this, [this] {
            if (auto* page = dynamic_cast<QWidget*>(selectedPage()))
                if (auto* action = page->findChild<QAction*>("actionAddItem")) action->trigger();
        });
        takeCentralWidget();
        layout->addWidget(m_container, 1);
        m_editorStack = new QStackedWidget(this);
        m_editorStack->addWidget(body);
        setCentralWidget(m_editorStack);
        connect(m_container, &PageContainer::selectedPageChanged, this, [this](BasePage*, BasePage* selected) {
            updateEditorNavigation(selected ? selected->id() : QString());
        });
        updateEditorNavigation("mods");
    }

    // Add custom buttons to the page container layout.
    {
        auto horizontalLayout = new QHBoxLayout(this);
        horizontalLayout->setObjectName(QStringLiteral("horizontalLayout"));
        horizontalLayout->setContentsMargins(0, 0, 6, 6);

        auto btnHelp = new QPushButton(this);
        btnHelp->setText(tr("Help"));
        horizontalLayout->addWidget(btnHelp);
        btnHelp->setVisible(!m_embedded);
        connect(btnHelp, &QPushButton::clicked, m_container, &PageContainer::help);

        auto spacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
        horizontalLayout->addSpacerItem(spacer);

        m_launchButton = new QToolButton(this);
        m_launchButton->setText(tr("&Launch"));
        m_launchButton->setToolTip(tr("Launch the instance"));
        m_launchButton->setPopupMode(QToolButton::MenuButtonPopup);
        m_launchButton->setMinimumWidth(80);  // HACK!!
        if (!m_embedded) horizontalLayout->addWidget(m_launchButton);
        else {
            m_launchButton->hide();
            m_pxlLaunchButton = new QPushButton(tr("Play"), this);
            m_pxlLaunchButton->setObjectName("instancePlay");
            m_pxlLaunchButton->setIcon(Pxl::primaryIcon("play"));
            m_pxlLaunchButton->setProperty("pxlPrimary", true);
            horizontalLayout->addWidget(m_pxlLaunchButton);
            horizontalLayout->addSpacing(8);
            connect(m_pxlLaunchButton, &QPushButton::clicked, this, [this] { if (saveAll()) APPLICATION->launch(m_instance); });
            auto* options = new QPushButton(this);
            options->setIcon(QIcon(":/pxl/icons/dots-three-vertical.svg"));
            options->setToolTip(tr("Launch options and help"));
            options->setAccessibleName(tr("Launch options and help"));
            horizontalLayout->addWidget(options);
            connect(options, &QPushButton::clicked, this, [this, options] {
                Pxl::FrostedMenu menu(this);
                menu.setParent(nullptr, menu.windowFlags());
                connect(this, &QObject::destroyed, &menu, &QMenu::close);
                auto* profilers = new Pxl::FrostedMenu(&menu);
                profilers->setTitle(tr("Advanced: profilers"));
                if (auto* source = m_launchButton->menu())
                    for (auto* command : source->actions()) {
                        if (command->isSeparator()) continue;
                        if (command->isCheckable()) Pxl::forwardMenuAction(profilers, command, command->text());
                        else Pxl::forwardMenuAction(&menu, command, command->text());
                    }
                menu.addSeparator();
                if (!profilers->actions().isEmpty()) menu.addMenu(profilers);
                menu.addAction(tr("Help"), m_container, &PageContainer::help);
                menu.exec(options->mapToGlobal(QPoint(0, options->height())));
            });
        }
        connect(m_launchButton, &QPushButton::clicked, this, [this] {
            if (!m_embedded || saveAll()) APPLICATION->launch(m_instance);
        });

        m_killButton = new QPushButton(this);
        m_killButton->setText(tr("&Kill"));
        m_killButton->setToolTip(tr("Kill the running instance"));
        m_killButton->setShortcut(QKeySequence(tr("Ctrl+K")));
        horizontalLayout->addWidget(m_killButton);
        connect(m_killButton, &QPushButton::clicked, this, [this] { APPLICATION->kill(m_instance); });

        updateButtons();

        m_closeButton = new QPushButton(this);
        m_closeButton->setText(tr("Close"));
        horizontalLayout->addWidget(m_closeButton);
        m_closeButton->setVisible(!m_embedded);
        connect(m_closeButton, &QPushButton::clicked, this, [this] {
            if (m_embedded) emit backRequested();
            else close();
        });

        m_container->addButtons(horizontalLayout);

        connect(m_instance, &BaseInstance::profilerChanged, this, &InstanceWindow::updateButtons);
        connect(m_instance, &BaseInstance::propertiesChanged, this, &InstanceWindow::updateButtons);
        connect(APPLICATION, &Application::globalSettingsApplied, this, &InstanceWindow::updateButtons);
    }

    // restore window state
    if (!m_embedded) {
        auto base64State = APPLICATION->settings()->get("ConsoleWindowState").toString().toUtf8();
        restoreState(QByteArray::fromBase64(base64State));
        auto base64Geometry = APPLICATION->settings()->get("ConsoleWindowGeometry").toString().toUtf8();
        restoreGeometry(QByteArray::fromBase64(base64Geometry));
    }

    // set up instance and launch process recognition
    {
        auto launchTask = m_instance->getLaunchTask();
        instanceLaunchTaskChanged(launchTask);
        connect(m_instance, &BaseInstance::launchTaskChanged, this, &InstanceWindow::instanceLaunchTaskChanged);
        connect(m_instance, &BaseInstance::runningStatusChanged, this, &InstanceWindow::runningStateChanged);
    }

    // set up instance destruction detection
    {
        connect(m_instance, &BaseInstance::statusChanged, this, &InstanceWindow::on_instanceStatusChanged);
    }

    // add ourself as the modpack page's instance window
    {
        static_cast<ManagedPackPage*>(m_container->getPage("managed_pack"))->setInstanceWindow(this);
    }

    if (!m_embedded) show();
}

void InstanceWindow::on_instanceStatusChanged(BaseInstance::Status, BaseInstance::Status newStatus)
{
    if (newStatus == BaseInstance::Status::Gone) {
        m_doNotSave = true;
        close();
    }
}

void InstanceWindow::updateButtons()
{
    m_launchButton->setEnabled(m_instance->canLaunch());
    m_killButton->setEnabled(m_instance->isRunning());
    if (m_embedded) {
        const bool running = m_instance->isRunning();
        m_launchButton->hide();
        m_pxlLaunchButton->setVisible(!running);
        m_pxlLaunchButton->setEnabled(m_instance->canLaunch());
        m_launchButton->setText(tr("Play"));
        m_launchButton->setIcon(QIcon(":/pxl/icons/play.svg"));
        m_launchButton->setProperty("pxlPrimary", true);
        m_killButton->setVisible(running);
        m_killButton->setText(tr("Stop"));
        m_killButton->setIcon(QIcon(":/pxl/icons/stop.svg"));
        m_killButton->setProperty("pxlStop", true);
        for (auto* button : {static_cast<QWidget*>(m_launchButton), static_cast<QWidget*>(m_killButton)}) {
            button->style()->unpolish(button);
            button->style()->polish(button);
        }
    }

    QMenu* launchMenu = m_launchButton->menu();
    if (launchMenu)
        launchMenu->clear();
    else
        launchMenu = new QMenu(this);
    m_instance->populateLaunchMenu(launchMenu);
    m_launchButton->setMenu(launchMenu);
}

void InstanceWindow::instanceLaunchTaskChanged(LaunchTask* proc)
{
    m_proc = proc;
}

void InstanceWindow::runningStateChanged(bool running)
{
    updateButtons();
    m_container->refreshContainer();
    if (running) {
        selectPage("log");
    }
}

void InstanceWindow::closeEvent(QCloseEvent* event)
{
    bool proceed = true;
    if (m_contentBrowser && !m_contentBrowser->prepareToLeave()) {
        event->ignore();
        return;
    }
    if (!m_doNotSave) {
        proceed &= m_container->prepareToClose();
    }

    if (!proceed) {
        event->ignore();
        return;
    }

    if (!m_embedded) {
        APPLICATION->settings()->set("ConsoleWindowState", QString::fromUtf8(saveState().toBase64()));
        APPLICATION->settings()->set("ConsoleWindowGeometry", QString::fromUtf8(saveGeometry().toBase64()));
    }
    emit isClosing();
    event->accept();
}

bool InstanceWindow::saveAll()
{
    if (m_contentBrowser && !m_contentBrowser->prepareToLeave()) return false;
    return m_doNotSave || m_container->saveAll();
}

void InstanceWindow::browseContent(const QString& category)
{
    auto* instance = dynamic_cast<MinecraftInstance*>(m_instance);
    if (!m_embedded || !instance || !m_editorStack) return;
    if (!m_contentBrowser) {
        m_contentBrowser = new ContentBrowser(instance, m_editorStack);
        m_editorStack->addWidget(m_contentBrowser);
        connect(m_contentBrowser, &ContentBrowser::installedContentRequested, this, [this] { m_editorStack->setCurrentIndex(0); });
        connect(m_contentBrowser, &ContentBrowser::setupRequested, this, [this] {
            m_editorStack->setCurrentIndex(0);
            selectPage("version");
        });
        connect(m_contentBrowser, &ContentBrowser::worldsRequested, this, [this] {
            m_editorStack->setCurrentIndex(0);
            selectPage("worlds");
        });
    }
    if (!m_contentBrowser->browse(category)) return;
    m_editorStack->setCurrentWidget(m_contentBrowser);
}

QString InstanceWindow::instanceId()
{
    return m_instance->id();
}

bool InstanceWindow::selectPage(QString pageId)
{
    const auto* previous = selectedPage();
    const bool selected = m_container->selectPage(pageId);
    if (m_embedded && selectedPage()) updateEditorNavigation(selectedPage()->id());
    if (m_embedded && previous != selectedPage())
        Pxl::animatePage(dynamic_cast<QWidget*>(selectedPage()));
    return selected;
}

void InstanceWindow::refreshContainer()
{
    m_container->refreshContainer();
}

BasePage* InstanceWindow::selectedPage() const
{
    return m_container->selectedPage();
}

bool InstanceWindow::requestClose()
{
    if (m_embedded) {
        if (!saveAll()) return false;
        emit backRequested();
        return true;
    }
    if (m_container->prepareToClose()) {
        close();
        return true;
    }
    return false;
}

void InstanceWindow::updateEditorNavigation(const QString& page)
{
    if (!m_embedded) return;
    bool common = false;
    for (auto* button : m_editorTabs) {
        const bool selected = button->property("pxlPage").toString() == page;
        button->setChecked(selected);
        common |= selected && page != "version";
    }
    const bool content = page == "mods" || page == "resourcepacks" || page == "texturepacks" || page == "shaderpacks";
    const auto* widget=dynamic_cast<QWidget*>(selectedPage());
    const bool ownControls=widget && widget->property("pxlInstalledContent").toBool();
    m_browseContent->setVisible(content && !ownControls);
    m_addContent->setVisible(content && !ownControls);
    m_container->setPageListVisible(false);
    m_container->setPageHeadingVisible(false);
    m_advancedChoice->setVisible(!common);
    if (!common) {
        const QSignalBlocker blocker(m_advancedChoice);
        m_advancedChoice->setCurrentIndex(m_advancedChoice->findData(page));
    }
    if (!common && !m_editorTabs.isEmpty()) m_editorTabs.last()->setChecked(true);
}

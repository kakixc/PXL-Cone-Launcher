/* Copyright 2013-2021 MultiMC Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "PageDialog.h"

#include <QDialogButtonBox>
#include <QKeyEvent>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QMessageBox>
#include <QAbstractButton>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QAbstractSlider>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QScrollArea>
#include <QCloseEvent>

#include "Application.h"
#include "settings/SettingsObject.h"

#include "ui/widgets/PageContainer.h"
#include "ui/pxl/DialogOverlay.h"
#include "ui/pxl/LibraryControls.h"
#include "ui/pxl/ToggleSwitch.h"

PageDialog::PageDialog(BasePageProvider* pageProvider, QString defaultId, QWidget* parent, bool embedded) : QDialog(parent), m_embedded(embedded)
{
    setWindowTitle(pageProvider->dialogTitle());
    m_container = new PageContainer(pageProvider, std::move(defaultId), this);

    auto* mainLayout = new QVBoxLayout(this);
    if (embedded) {
        setWindowFlags(Qt::Widget);
        setProperty("pxlFlow", true);
        setProperty("pxlSettings", true);
        setStyleSheet("PageDialog { background: transparent; } PageDialog QGroupBox { border: 1px solid #383e47; border-radius: 10px; margin-top: 16px; padding: 16px 12px 12px; } PageDialog QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 5px; font-weight: 600; }");
        m_container->useConeStyle();
        if (m_container->getPage("accounts")) {
            m_container->setExcludedPage("accounts");
            if (m_container->selectedPage() && m_container->selectedPage()->id()=="accounts") m_container->selectPage("launcher-settings");
        }
        for (auto* area : m_container->findChildren<QScrollArea*>()) {
            area->viewport()->setAutoFillBackground(false);
            if (area->widget()) area->widget()->setAutoFillBackground(false);
        }
        mainLayout->setContentsMargins(24, 8, 24, 20);
        mainLayout->setSpacing(16);
        auto* search = new Pxl::LibrarySearch(this);
        search->setObjectName("pxlSettingsSearch");
        search->setMaximumWidth(500);
        search->setMinimumWidth(0);
        search->setPlaceholderText(tr("Search settings"));
        search->setClearButtonEnabled(true);
        auto* topControls = new QWidget(this);
        topControls->setObjectName("pxlSettingsToolbar");
        topControls->setProperty("pxlTransientControl",true);
        auto* toolbar = new QHBoxLayout(topControls);
        toolbar->setContentsMargins(0,0,0,0);
        toolbar->setSpacing(16);
        auto* advancedMode = new Pxl::ToggleSwitch(tr("Advanced mode"), this);
        advancedMode->setObjectName("pxlAdvancedSettingsMode");
        advancedMode->setProperty("pxlDisclosure", true);
        advancedMode->setToolTip(tr("Show technical settings, services, tools and proxy configuration."));
        advancedMode->setChecked(APPLICATION_DYN && APPLICATION_DYN->settings()->get("WorkspaceAdvancedSettings").toBool());
        advancedMode->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
        advancedMode->ensurePolished();
        // Equal flanks center the search in the content column, independently of the mode control.
        toolbar->addSpacerItem(new QSpacerItem(advancedMode->sizeHint().width()+toolbar->spacing(),0,QSizePolicy::Fixed,QSizePolicy::Minimum));
        toolbar->addSpacerItem(new QSpacerItem(0,0,QSizePolicy::Expanding,QSizePolicy::Minimum));
        toolbar->addWidget(search,1);
        toolbar->addSpacerItem(new QSpacerItem(0,0,QSizePolicy::Expanding,QSizePolicy::Minimum));
        toolbar->addWidget(advancedMode);
        m_container->setTopControls(topControls);
        m_container->setAdvancedMode(advancedMode->isChecked());
        connect(advancedMode, &QCheckBox::toggled, this, [this](bool advanced) {
            m_container->setAdvancedMode(advanced);
            if (auto* app = APPLICATION_DYN) app->settings()->set("WorkspaceAdvancedSettings", advanced);
        });
        connect(search, &QLineEdit::textChanged, m_container, &PageContainer::setSearchText);
        mainLayout->addWidget(m_container, 1);
        auto* footer = new QHBoxLayout;
        auto* hint = new QLabel(tr("Apply your changes before leaving."), this);
        hint->setProperty("pxlFlowHint", true);
        footer->addWidget(hint);
        footer->addStretch();
        auto* apply = new QPushButton(tr("Apply changes"), this);
        m_applyButton = apply;
        apply->setObjectName("pxlApplySettings");
        setDirty(false);
        footer->addWidget(apply);
        mainLayout->addLayout(footer);
        connect(apply, &QPushButton::clicked, this, [this] { applyChanges(); });
        auto transient = [this](QWidget* control) {
            for (auto* widget=control; widget && widget!=m_container; widget=widget->parentWidget())
                if (widget->property("pxlTransientControl").toBool()) return true;
            return false;
        };
        for (auto* edit : m_container->findChildren<QLineEdit*>())
            if (!transient(edit)) connect(edit, &QLineEdit::textChanged, this, [this] { setDirty(true); });
        for (auto* button : m_container->findChildren<QAbstractButton*>())
            if (!transient(button) && !button->property("pxlDisclosure").toBool())
                connect(button, &QAbstractButton::clicked, this, [this] { setDirty(true); });
        for (auto* combo : m_container->findChildren<QComboBox*>())
            connect(combo, qOverload<int>(&QComboBox::activated), this, [this] { setDirty(true); });
        for (auto* spin : m_container->findChildren<QSpinBox*>())
            connect(spin, &QSpinBox::valueChanged, this, [this] { setDirty(true); });
        for (auto* spin : m_container->findChildren<QDoubleSpinBox*>())
            connect(spin, &QDoubleSpinBox::valueChanged, this, [this] { setDirty(true); });
        for (auto* slider : m_container->findChildren<QAbstractSlider*>())
            if (!qobject_cast<QScrollBar*>(slider))
                connect(slider, &QAbstractSlider::valueChanged, this, [this] { setDirty(true); });
        for (auto* edit : m_container->findChildren<QTextEdit*>())
            connect(edit, &QTextEdit::textChanged, this, [this] { setDirty(true); });
        for (auto* edit : m_container->findChildren<QPlainTextEdit*>())
            connect(edit, &QPlainTextEdit::textChanged, this, [this] { setDirty(true); });
        return;
    }

    auto* focusStealer = new QPushButton(this);
    mainLayout->addWidget(focusStealer);
    focusStealer->setDefault(true);
    focusStealer->hide();

    mainLayout->addWidget(m_container);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    setLayout(mainLayout);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Help | QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("&OK"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("&Cancel"));
    buttons->button(QDialogButtonBox::Help)->setText(tr("Help"));
    buttons->setContentsMargins(0, 0, 6, 6);
    m_container->addButtons(buttons);

    connect(buttons->button(QDialogButtonBox::Ok), &QPushButton::clicked, this, &PageDialog::accept);
    connect(buttons->button(QDialogButtonBox::Cancel), &QPushButton::clicked, this, &PageDialog::reject);
    connect(buttons->button(QDialogButtonBox::Help), &QPushButton::clicked, m_container, &PageContainer::help);

    restoreGeometry(QByteArray::fromBase64(APPLICATION->settings()->get("PagedGeometry").toString().toUtf8()));
}

void PageDialog::accept()
{
    if (handleClose())
        QDialog::accept();
}

void PageDialog::closeEvent(QCloseEvent* event)
{
    if (m_embedded) {
        if (requestLeave()) event->accept();
        else event->ignore();
        return;
    }
    if (handleClose())
        QDialog::closeEvent(event);
}

void PageDialog::setDirty(bool dirty)
{
    m_dirty = dirty;
    if (!m_applyButton) return;
    m_applyButton->setProperty("pxlPrimary", dirty);
    m_applyButton->setEnabled(dirty);
    m_applyButton->style()->unpolish(m_applyButton);
    m_applyButton->style()->polish(m_applyButton);
    m_applyButton->update();
}

bool PageDialog::applyChanges()
{
    {
        std::unique_ptr<SettingsObject::Lock> lock;
        if (auto* app = APPLICATION_DYN) lock = std::make_unique<SettingsObject::Lock>(app->settings());
        if (!m_container->saveAll()) return false;
    }
    emit applied();
    setDirty(false);
    return true;
}

bool PageDialog::requestLeave()
{
    if (m_dirty) {
        QMessageBox confirmation(QMessageBox::Question, tr("Unsaved settings"), tr("Apply your changes before leaving?"),
                                  QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, this);
        confirmation.setDefaultButton(QMessageBox::Save);
        confirmation.setProperty("pxlPreferredSize", QSize(520, 220));
        const auto choice = Pxl::DialogOverlay::run(&confirmation, this);
        if (choice != QMessageBox::Save && choice != QMessageBox::Discard) return false;
        if (choice == QMessageBox::Save && !applyChanges()) return false;
    }
    setDirty(false);
    deleteLater();
    return true;
}

void PageDialog::selectPage(const QString& page)
{
    m_container->selectPage(page);
}

bool PageDialog::handleClose()
{
    qDebug() << "Paged dialog close requested";
    if (!m_container->prepareToClose())
        return false;

    qDebug() << "Paged dialog close approved";
    APPLICATION->settings()->set("PagedGeometry", QString::fromUtf8(saveGeometry().toBase64()));
    qDebug() << "Paged dialog geometry saved";

    emit applied();
    return true;
}

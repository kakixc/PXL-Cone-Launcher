// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "Application.h"
#include "ui/MainWindow.h"
#include "BuildConfig.h"
#include "SettingsCards.h"
#include "updater/ReleaseUpdateChecker.h"
#include "ui/pages/BasePage.h"
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSysInfo>
#include <QVBoxLayout>
#include <QTimer>
#include <functional>

namespace Pxl {
class AboutPage final : public QWidget, public BasePage {
    static QString text(const char* value) { return QCoreApplication::translate("Pxl::AboutPage",value); }
public:
    explicit AboutPage(QWidget* parent = nullptr, std::function<void()> check = {}) : QWidget(parent)
    {
        setObjectName("pxlAboutPage");
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(24,24,24,24);
        layout->setSpacing(14);
        layout->addStretch(1);
        auto* logo = new QLabel(this);
        logo->setPixmap(QIcon(":/pxl/cone-icon.svg").pixmap(96,96));
        logo->setAlignment(Qt::AlignCenter);
        layout->addWidget(logo);
        auto* title = new QLabel("PXL Cone Launcher",this);
        title->setAlignment(Qt::AlignCenter);
        title->setStyleSheet("font-size:26px; font-weight:700; background:transparent;");
        layout->addWidget(title);
        auto* version = new QLabel(this);
        version->setObjectName("aboutVersion");
        version->setAlignment(Qt::AlignCenter);
        version->setStyleSheet("color:#b5bdc9; background:transparent;");
        layout->addWidget(version);
        auto* information = settingsCard(this,{},{});
        information->setMinimumWidth(400);
        information->setMaximumWidth(600);
        information->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
        // The centered title above replaces the shared card's optional title.
        auto* fields = static_cast<QVBoxLayout*>(information->layout());
        if (auto* heading = fields->takeAt(0)) { delete heading->widget(); delete heading; }
        auto* grid = new QGridLayout;
        grid->setHorizontalSpacing(24); grid->setVerticalSpacing(14);
        fields->addLayout(grid);
        const QStringList values{
            BuildConfig.GIT_COMMIT.left(10) + " · " + BuildConfig.BUILD_DATE,
            QSysInfo::prettyProductName() + " · " + QSysInfo::currentCpuArchitecture(),
            QString::fromLatin1(qVersion())};
        QStringList keys{text("Build"),text("System"),text("Qt version")};
        for (int row=0; row<keys.size(); ++row) {
            auto* label = new QLabel(keys[row],information);
            label->setProperty("aboutKey",row);
            label->setStyleSheet("color:#a4adba;");
            grid->addWidget(label,row,0);
            auto* value = new QLabel(values[row],information);
            value->setTextFormat(Qt::PlainText); value->setWordWrap(true);
            value->setTextInteractionFlags(Qt::TextSelectableByMouse);
            grid->addWidget(value,row,1);
        }
        grid->setColumnStretch(1,1);
        layout->addWidget(information,0,Qt::AlignHCenter);
        m_check = new QPushButton(this);
        m_check->setObjectName("aboutCheckUpdates");
        m_check->setProperty("pxlDisclosure",true);
        m_check->setIcon(QIcon(":/pxl/icons/arrows-clockwise.svg"));
        m_check->setCursor(Qt::PointingHandCursor);
        m_check->setEnabled(bool(check) || (APPLICATION_DYN && APPLICATION_DYN->updaterEnabled()));
        connect(m_check,&QPushButton::clicked,this,[check=std::move(check)] {
            if (check) check();
            else if (auto* app=APPLICATION_DYN) app->triggerUpdateCheck();
        });
        layout->addWidget(m_check,0,Qt::AlignHCenter);
        m_welcome = new QPushButton(this);
        m_welcome->setObjectName("aboutWelcomeSetup");
        m_welcome->setProperty("pxlDisclosure", true);
        m_welcome->setEnabled(APPLICATION_DYN != nullptr);
        connect(m_welcome, &QPushButton::clicked, this, [] {
            if (auto* app = APPLICATION_DYN) {
                auto* window = app->showMainWindow();
                QTimer::singleShot(0, window, &MainWindow::showWelcomeSetup);
            }
        });
        layout->addWidget(m_welcome, 0, Qt::AlignHCenter);
        layout->addStretch(2);
        retranslate();
    }
    QString id() const override { return "about"; }
    QString displayName() const override { return text("About"); }
    QIcon icon() const override { return QIcon(":/pxl/cone-icon.svg"); }
    void retranslate() override
    {
        m_check->setText(text("Check for updates"));
        m_welcome->setText(text("First steps & setup"));
        const QStringList keys{text("Build"),text("System"),text("Qt version")};
        for (auto* label : findChildren<QLabel*>()) {
            if (label->property("aboutKey").isValid()) label->setText(keys.value(label->property("aboutKey").toInt()));
        }
        findChild<QLabel*>("aboutVersion")->setText(BuildConfig.RELEASE_VERSION == "0.0.0-dev" ? text("Development preview") : BuildConfig.RELEASE_VERSION);
    }
private:
    QPushButton* m_check = nullptr;
    QPushButton* m_welcome = nullptr;
};
}

// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "Animations.h"
#include "SettingsCards.h"
#include "ToggleSwitch.h"
#include "WelcomeTour.h"
#include <settings/SettingsObject.h>
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QButtonGroup>
#include <QLocale>
#include <QTimer>
#include <functional>
#include <memory>

namespace Pxl {
class WelcomeTourStep final : public QPushButton {
public:
    using QPushButton::QPushButton;
    void setProgress(qreal value) { m_progress = qBound(0.0, value, 1.0); update(); }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing);
        const QRectF area = QRectF(rect()).adjusted(.5, .5, -.5, -.5);
        QPainterPath shape; shape.addRoundedRect(area, 10, 10);
        painter.fillPath(shape, underMouse() ? QColor(255, 255, 255, 24) : QColor(255, 255, 255, 12));
        painter.save(); painter.setClipPath(shape);
        painter.fillRect(QRectF(0, 0, width() * m_progress, height()), QColor(242, 163, 101, 58)); painter.restore();
        painter.setPen(QPen(isChecked() || hasFocus() ? QColor(242, 163, 101, 145) : QColor(255, 255, 255, 18), 1));
        painter.drawPath(shape);
        painter.setFont(font()); painter.setPen(QColor("#edf0f4")); painter.drawText(rect(), Qt::AlignCenter, text());
    }
private:
    qreal m_progress = 0;
};

inline QWidget* welcomeSetupSurface(QWidget* workspace)
{
    if (auto* modal = QApplication::activeModalWidget(); modal && modal->isVisible()) return modal;
    return workspace;
}
inline bool needsWelcomeSetup(SettingsObject* settings)
{
    return settings->get("PXLWelcomeRevision").toInt() < 1;
}

class WelcomeSetup final : public QDialog {
public:
    WelcomeSetup(SettingsObject* settings, bool automaticJavaAvailable,
                 std::function<void(const QString&)> chooseLanguage,
                 std::function<QWidget*(QWidget*)> accounts, QWidget* parent = nullptr)
        : QDialog(parent), m_settings(settings), m_chooseLanguage(std::move(chooseLanguage)), m_accounts(std::move(accounts)), m_automaticJavaAvailable(automaticJavaAvailable)
    {
        setObjectName("pxlWelcomeSetup");
        setProperty("pxlFlow", true);
        setProperty("pxlEmbeddedAuth", true);
        setProperty("pxlAnimateLifecycle", true);
        setProperty("pxlPreferredSize", QSize(760, 650));
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(26, 24, 26, 22); root->setSpacing(16);
        auto* header = new QHBoxLayout;
        m_brandIcon = new QLabel(this); m_brandIcon->setPixmap(QIcon(":/pxl/cone-icon.svg").pixmap(32, 32)); header->addWidget(m_brandIcon);
        m_title = new QLabel("PXL Cone", this); m_title->setStyleSheet("font-size:18px; font-weight:600;"); header->addWidget(m_title, 1);
        header->addStretch();
        m_skip = button("Skip setup", "welcomeSkip", this); header->addWidget(m_skip); root->addLayout(header);
        m_step = new QLabel(this); m_step->setProperty("pxlFlowHint", true); root->addWidget(m_step);
        m_stack = new QStackedWidget(this); m_stack->setObjectName("welcomeSteps"); root->addWidget(m_stack, 1);

        auto* language = page();
        auto* languageLayout = static_cast<QVBoxLayout*>(language->layout());
        languageLayout->setSpacing(12);
        languageLayout->addStretch(1);
        auto* largeLogo = new QLabel(language);
        largeLogo->setObjectName("welcomeHeroLogo");
        largeLogo->setPixmap(QIcon(":/pxl/cone-icon.svg").pixmap(160, 160));
        largeLogo->setFixedHeight(160);
        largeLogo->setAlignment(Qt::AlignCenter); languageLayout->addWidget(largeLogo);
        auto* welcome = label("Welcome to PXL Cone", language);
        welcome->setObjectName("welcomeHeroTitle"); welcome->setAlignment(Qt::AlignCenter);
        welcome->setStyleSheet("font-size:30px; font-weight:700; background:transparent;"); languageLayout->addWidget(welcome);
        auto* slogan = label("Your Minecraft. Your space.", language);
        slogan->setAlignment(Qt::AlignCenter);
        slogan->setStyleSheet("font-size:18px; font-weight:600; color:#f2b784; background:transparent;"); languageLayout->addWidget(slogan);
        auto* intro = label("Instances, mods and favourite worlds — together, with room to make it yours.", language);
        intro->setAlignment(Qt::AlignCenter); intro->setProperty("pxlFlowHint", true); languageLayout->addWidget(intro);
        auto* benefits = new QWidget(language); auto* benefitLayout = new QVBoxLayout(benefits);
        benefits->setObjectName("welcomeBenefits");
        benefits->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
        benefitLayout->setContentsMargins(16, 4, 16, 4); benefitLayout->setSpacing(9);
        const QList<QPair<QString, const char*>> entries{
            {"squares-four", "Organise your instances and launch in a click"},
            {"plus", "Discover mods, resource packs and shaders"},
            {"arrow-square-in", "Bring your existing Minecraft setup with you"}};
        for (const auto& entry : entries) {
            auto* line = new QHBoxLayout; line->setSpacing(10);
            auto* icon = new QLabel(benefits); icon->setPixmap(QIcon(":/pxl/icons/" + entry.first + ".svg").pixmap(20, 20));
            icon->setFixedSize(22, 22); line->addWidget(icon);
            auto* detail = label(entry.second, benefits); detail->setWordWrap(false); detail->setStyleSheet("font-size:13px; color:#bdc5d0; background:transparent;"); line->addWidget(detail, 1);
            benefitLayout->addLayout(line);
        }
        benefits->setMaximumWidth(470);
        languageLayout->addWidget(benefits, 0, Qt::AlignHCenter);
        auto* languages = new QHBoxLayout;
        const auto selected = settings->get("Language").toString();
        m_language = selected.isEmpty() ? (QLocale::system().language() == QLocale::Russian ? "ru" : "en_US") : selected;
        for (const auto& entry : QList<QPair<QString, QString>>{{"en_US", "English"}, {"ru", QString::fromUtf8("Русский")}}) {
            auto* choice = new QPushButton(entry.second, language);
            choice->setObjectName("welcomeLanguage_" + entry.first);
            choice->setCheckable(true); choice->setAutoExclusive(true); choice->setProperty("pxlMode", true);
            choice->setMinimumHeight(50); choice->setChecked(m_language == entry.first);
            languages->addWidget(choice);
            connect(choice, &QPushButton::clicked, this, [this, key = entry.first] {
                changeLanguage(key);
            });
        }
        languageLayout->addLayout(languages);
        languageLayout->addStretch(1);

        auto* account = page(); m_accountLayout = static_cast<QVBoxLayout*>(account->layout());
        m_accountLayout->addWidget(label("Your Minecraft profile", account, true));
        m_accountLayout->addWidget(label("Sign in now or continue without an account. You can add one before playing.", account));
        m_accountLayout->addStretch();

        auto* runtime = page(); auto* runtimeLayout = static_cast<QVBoxLayout*>(runtime->layout());
        runtimeLayout->addWidget(label("The right Java for each instance", runtime, true));
        runtimeLayout->addWidget(label("Minecraft versions need different runtimes. PXL can choose and download the required Java when you launch a game.", runtime));
        auto* javaCard = settingsCard(runtime, welcomeText("Automatic Java"), {});
        javaCard->findChild<QLabel*>()->setProperty("welcomeText", "Automatic Java");
        auto* javaLayout = static_cast<QVBoxLayout*>(javaCard->layout());
        m_java = new ToggleSwitch(welcomeText("Select and download Java automatically"), javaCard);
        m_java->setObjectName("welcomeAutomaticJava"); m_java->setEnabled(automaticJavaAvailable);
        m_java->setChecked(automaticJavaAvailable && settings->get("AutomaticJavaDownload").toBool() && settings->get("AutomaticJavaSwitch").toBool());
        connect(m_java, &QCheckBox::toggled, this, [this] { m_javaEdited = true; });
        javaLayout->addWidget(m_java);
        auto* javaHint = label(automaticJavaAvailable ? "Your existing manual Java path is kept. You can adjust runtimes per instance in Settings." : "Automatic downloads are unavailable in this build. Choose a local Java installation in Settings before playing.", javaCard);
        javaHint->setProperty("pxlFlowHint", true); javaLayout->addWidget(javaHint);
        runtimeLayout->addWidget(javaCard);
        m_javaStatus = new QLabel(runtime); m_javaStatus->setObjectName("welcomeJavaStatus");
        m_javaStatus->setWordWrap(true); m_javaStatus->setProperty("pxlFlowHint", true); runtimeLayout->addWidget(m_javaStatus);
        runtimeLayout->addStretch();

        auto* tour = page(); auto* tourLayout = static_cast<QVBoxLayout*>(tour->layout());
        tourLayout->setSpacing(10);
        tourLayout->addWidget(label("A quick look around", tour, true));
        auto* demos = new QHBoxLayout; demos->setSpacing(6);
        auto* group = new QButtonGroup(this); group->setExclusive(true);
        const char* demoNames[] = {"Create", "Content", "Downloads", "Screenshots"};
        for (int i = 0; i < 4; ++i) {
            auto* choice = new WelcomeTourStep(welcomeText(demoNames[i]), tour);
            choice->setObjectName("welcomeDemo" + QString::number(i));
            choice->setProperty("welcomeText", demoNames[i]); choice->setAutoDefault(false);
            choice->setMinimumHeight(36); choice->setCursor(Qt::PointingHandCursor);
            choice->setCheckable(true); choice->setProperty("pxlMode", true); group->addButton(choice, i); demos->addWidget(choice, 1);
            m_demoButtons.append(choice);
            choice->setChecked(i == 0);
        }
        tourLayout->addLayout(demos);
        m_demoTitle = new QLabel(tour); m_demoTitle->setWordWrap(true); m_demoTitle->setStyleSheet("font-size:16px; font-weight:600;"); tourLayout->addWidget(m_demoTitle);
        m_tour = new WelcomeTour(tour); tourLayout->addWidget(m_tour);
        m_caption = new QLabel(tour); m_caption->setWordWrap(true); m_caption->setProperty("pxlFlowHint", true); tourLayout->addWidget(m_caption);
        auto* repeat = button("Replay demo", "welcomeReplay", tour); tourLayout->addWidget(repeat, 0, Qt::AlignLeft);
        m_advance = new QTimer(this); m_advance->setObjectName("welcomeAdvance"); m_advance->setSingleShot(true); m_advance->setInterval(450);
        connect(m_advance, &QTimer::timeout, this, [this] {
            if (currentStep() == 3 && m_tour->isVisible() && m_demo < 3) selectDemo(m_demo + 1);
        });
        m_tour->progressChanged = [this](qreal value) {
            m_demoProgress[m_demo] = value;
            for (int i = 0; i < m_demoButtons.size(); ++i) m_demoButtons[i]->setProgress(m_demoProgress[i]);
        };
        m_tour->finished = [this] { if (currentStep() == 3 && m_demo < 3) m_advance->start(); };
        connect(repeat, &QPushButton::clicked, this, [this] { selectDemo(m_demo); });
        connect(group, &QButtonGroup::idClicked, this, [this](int id) { selectDemo(id); });
        tourLayout->addStretch();

        auto* footer = new QHBoxLayout;
        m_back = button("Back", "welcomeBack", this); footer->addWidget(m_back); footer->addStretch();
        m_next = button("Next", "welcomeNext", this); m_next->setProperty("pxlPrimary", true); m_next->setDefault(true); footer->addWidget(m_next);
        root->addLayout(footer);
        connect(m_back, &QPushButton::clicked, this, [this] { setStep(m_stack->currentIndex() - 1); });
        connect(m_next, &QPushButton::clicked, this, [this] {
            if (m_stack->currentIndex() == 3) complete(false);
            else setStep(m_stack->currentIndex() + 1);
        });
        connect(m_skip, &QPushButton::clicked, this, [this] { complete(true); });
        retranslate(); setStep(0);
    }
    int currentStep() const { return m_stack->currentIndex(); }
    void setJavaVersions(const QStringList& versions) { m_javaVersions = versions; m_javaState = 1; updateJavaStatus(); }
    void setJavaDetectionFailed() { m_javaState = 2; updateJavaStatus(); }
protected:
    void done(int result) override
    {
        stopLanguageTransition();
        if (auto* overlay = dynamic_cast<DialogOverlay*>(parentWidget()); overlay &&
            overlay->closeAnimated([this, result] { finishClosing(result); })) return;
        finishClosing(result);
    }
    void finishClosing(int result)
    {
        if (result != QDialog::Accepted && m_chooseLanguage)
            m_chooseLanguage(m_settings->get("Language").toString());
        QDialog::done(result);
    }
    void changeEvent(QEvent* event) override
    {
        QDialog::changeEvent(event);
        if (event->type() == QEvent::LanguageChange) retranslate();
    }
private:
    QLabel* label(const char* source, QWidget* parent, bool heading = false)
    {
        auto* result = new QLabel(welcomeText(source), parent);
        result->setProperty("welcomeText", source); result->setWordWrap(true);
        result->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
        if (heading) result->setStyleSheet("font-size:22px; font-weight:700;");
        return result;
    }
    QPushButton* button(const char* source, const QString& name, QWidget* parent)
    {
        auto* result = new QPushButton(welcomeText(source), parent);
        result->setProperty("welcomeText", source); result->setObjectName(name);
        result->setAutoDefault(false); result->setMinimumHeight(36); result->setCursor(Qt::PointingHandCursor);
        return result;
    }
    QWidget* page()
    {
        auto* scroll = new QScrollArea(m_stack); scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame); scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setStyleSheet("QScrollArea, QScrollArea > QWidget > QWidget { background:transparent; border:none; }");
        auto* content = new QWidget(scroll); auto* layout = new QVBoxLayout(content);
        layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(18);
        scroll->setWidget(content); m_stack->addWidget(scroll);
        return content;
    }
    void setStep(int step)
    {
        step = qBound(0, step, 3);
        if (m_stack->currentIndex() == 0 && step != 0 && m_languageTransition && m_languageTransition->state() == QAbstractAnimation::Running) {
            stopLanguageTransition();
            if (m_chooseLanguage) m_chooseLanguage(m_language);
        }
        m_advance->stop();
        // Recreate only the presentation after returning from language choice;
        // profiles already added remain owned and saved by AccountList.
        if (step == 1 && m_accounts) {
            if (m_accountContent) delete m_accountContent;
            m_accountContent = m_accounts(m_accountLayout->parentWidget());
            m_accountContent->setMaximumHeight(300);
            m_accountLayout->insertWidget(2, m_accountContent);
        }
        m_stack->setCurrentIndex(step); retranslate();
        const QSize preferred[] = {{760, 740}, {760, 600}, {760, 480}, {760, 650}};
        setProperty("pxlPreferredSize", preferred[step]);
        layout()->invalidate();
        QCoreApplication::postEvent(this, new QEvent(QEvent::LayoutRequest));
        animatePage(m_stack->currentWidget());
        if (step == 3) selectDemo(m_demo);
        m_next->setFocus(Qt::OtherFocusReason);
    }
    void selectDemo(int demo)
    {
        m_advance->stop(); m_demo = qBound(0, demo, 3);
        m_demoButtons[m_demo]->setChecked(true); updateTour();
        animatePage(m_tour); m_tour->selectDemo(m_demo);
    }
    void changeLanguage(const QString& key)
    {
        stopLanguageTransition();
        m_language = key;
        const auto apply = [this, key] {
            if (m_chooseLanguage) m_chooseLanguage(key);
            retranslate();
        };
        auto* content = qobject_cast<QScrollArea*>(m_stack->widget(0))->widget();
        if (!content->isVisible() || !QApplication::isEffectEnabled(Qt::UI_AnimateMenu)) { apply(); return; }
        auto* effect = qobject_cast<QGraphicsOpacityEffect*>(content->graphicsEffect());
        if (!effect) { effect = new QGraphicsOpacityEffect(content); content->setGraphicsEffect(effect); }
        if (!m_languageTransition) {
            m_languageTransition = new QVariantAnimation(this);
            m_languageTransition->setObjectName("welcomeLanguageTransition");
            m_languageTransition->setDuration(240);
        }
        disconnect(m_languageTransition, nullptr, this, nullptr);
        effect->setEnabled(true); effect->setOpacity(1);
        m_languageTransition->setStartValue(0.0); m_languageTransition->setEndValue(1.0);
        auto changed = std::make_shared<bool>(false);
        connect(m_languageTransition, &QVariantAnimation::valueChanged, this, [effect, apply, changed](const QVariant& value) {
            const qreal progress = value.toReal();
            if (progress >= .5 && !*changed) { *changed = true; apply(); }
            effect->setOpacity(.25 + .75 * qAbs(2 * progress - 1));
        });
        connect(m_languageTransition, &QVariantAnimation::finished, this, [effect] { effect->setOpacity(1); effect->setEnabled(false); });
        m_languageTransition->start();
    }
    void stopLanguageTransition()
    {
        if (m_languageTransition) m_languageTransition->stop();
        if (m_stack && m_stack->count()) {
            auto* content = qobject_cast<QScrollArea*>(m_stack->widget(0))->widget();
            if (auto* effect = qobject_cast<QGraphicsOpacityEffect*>(content->graphicsEffect())) {
                effect->setOpacity(1); effect->setEnabled(false);
            }
        }
    }
    void updateJavaStatus()
    {
        m_javaStatus->setText(m_javaState == 0 ? welcomeText("Looking for installed Java…") : m_javaState == 2
            ? welcomeText("Java detection failed. You can check or select Java in Settings.")
            : !m_javaVersions.isEmpty() ? welcomeText("Found on this computer: Java %1").arg(m_javaVersions.join(", "))
            : m_automaticJavaAvailable ? welcomeText("No local Java installations found. Automatic setup can download the runtime when needed.")
            : welcomeText("No local Java installations found. Select Java in Settings before playing."));
    }
    void updateTour()
    {
        const char* titles[] = {"Build your next adventure", "Add something new", "Keep using the launcher", "Keep your favourite moments"};
        const char* captions[] = {
            "New instance → choose Minecraft and a loader → Create. Or import an existing pack from the library.",
            "Open an instance → Browse content → select additions → review and install. Compatibility depends on the game version and loader.",
            "Hide the installation panel to continue browsing. The bottom progress strip and Downloads & tasks keep the operation visible.",
            "Press F2 in Minecraft. Open Screenshots, enlarge a thumbnail, or use its context menu to set an instance cover."};
        m_demoTitle->setText(welcomeText(titles[m_demo])); m_caption->setText(welcomeText(captions[m_demo]));
        m_tour->setAccessibleName(welcomeText(titles[m_demo]));
        m_tour->setAccessibleDescription(welcomeText(captions[m_demo])); m_tour->update();
    }
    void retranslate()
    {
        for (auto* child : findChildren<QWidget*>()) {
            const auto source = child->property("welcomeText").toByteArray();
            if (source.isEmpty()) continue;
            if (auto* text = qobject_cast<QLabel*>(child)) text->setText(welcomeText(source.constData()));
            else if (auto* action = qobject_cast<QPushButton*>(child)) action->setText(welcomeText(source.constData()));
        }
        const int step = m_stack->currentIndex();
        const char* steps[] = {"Language", "Account", "Java", "Meet PXL Cone"};
        m_step->setText(welcomeText("Step %1 of %2 · %3").arg(step + 1).arg(4).arg(welcomeText(steps[qBound(0, step, 3)])));
        m_back->setVisible(step > 0);
        m_title->setVisible(step != 0); m_brandIcon->setVisible(step != 0);
        m_step->setAlignment(step == 0 ? Qt::AlignCenter : Qt::AlignLeft);
        m_next->setText(welcomeText(step == 3 ? "Start using PXL Cone" : step == 0 ? "Continue" : "Next"));
        m_java->setText(welcomeText("Select and download Java automatically"));
        m_java->setToolTip(m_java->text()); updateJavaStatus();
        updateTour();
    }
    void complete(bool skipped)
    {
        stopLanguageTransition();
        m_settings->set("Language", m_language);
        if (m_chooseLanguage) m_chooseLanguage(m_language);
        if (!skipped && m_java->isEnabled()) {
            if (m_javaEdited) {
                m_settings->set("AutomaticJavaDownload", m_java->isChecked());
                m_settings->set("AutomaticJavaSwitch", m_java->isChecked());
            }
            m_settings->set("UserAskedAboutAutomaticJavaDownload", true);
        }
        m_settings->set("PXLWelcomeRevision", 1);
        accept();
    }
    SettingsObject* m_settings = nullptr;
    std::function<void(const QString&)> m_chooseLanguage;
    std::function<QWidget*(QWidget*)> m_accounts;
    QStackedWidget* m_stack = nullptr;
    QVBoxLayout* m_accountLayout = nullptr;
    QWidget* m_accountContent = nullptr;
    QLabel *m_title = nullptr, *m_brandIcon = nullptr, *m_step = nullptr, *m_javaStatus = nullptr, *m_demoTitle = nullptr, *m_caption = nullptr;
    QPushButton *m_skip = nullptr, *m_back = nullptr, *m_next = nullptr;
    ToggleSwitch* m_java = nullptr;
    WelcomeTour* m_tour = nullptr;
    QList<WelcomeTourStep*> m_demoButtons;
    QTimer* m_advance = nullptr;
    QString m_language;
    int m_demo = 0;
    bool m_javaEdited = false;
    bool m_automaticJavaAvailable = true;
    QStringList m_javaVersions;
    int m_javaState = 0;
    QVariantAnimation* m_languageTransition = nullptr;
    qreal m_demoProgress[4] = {0, 0, 0, 0};
};
} // namespace Pxl

// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QCheckBox>
#include <QFontComboBox>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include "LibraryControls.h"
#include "SettingsCards.h"
#include "ToggleSwitch.h"

namespace Pxl {
// Present the existing setting model with the accepted PXL choice control.
inline ChoiceChip* appearanceChoice(QComboBox* source, QWidget* parent)
{
    auto* choice = new ChoiceChip(parent);
    choice->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    choice->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    choice->setModel(source->model());
    choice->setCurrentIndex(source->currentIndex());
    const QPointer<QComboBox> guardedSource = source;
    auto sync = [guardedSource, choice] {
        if (!guardedSource) { choice->setEnabled(false); return; }
        const QSignalBlocker blocker(choice);
        choice->setCurrentIndex(guardedSource->currentIndex());
    };
    QObject::connect(source, &QComboBox::currentIndexChanged, choice, sync);
    // Only user activation changes the original setting. Model reloads also
    // move combo indices; they must not apply a different theme or cat pack.
    QObject::connect(choice, &QComboBox::activated, source, [source](int index) { source->setCurrentIndex(index); });
    auto settle = [choice, sync] { QTimer::singleShot(0, choice, sync); };
    QObject::connect(source->model(), &QAbstractItemModel::modelReset, choice, settle);
    QObject::connect(source->model(), &QAbstractItemModel::rowsInserted, choice, settle);
    QObject::connect(source->model(), &QAbstractItemModel::rowsRemoved, choice, settle);
    return choice;
}

struct AppearanceControls {
    QComboBox *theme, *icons, *cats, *catScale;
    QPushButton *themeFolder, *iconsFolder, *catsFolder, *reload, *catPreview;
    QFontComboBox* logFont;
    QSpinBox* logSize;
    QSlider* catOpacity;
    QCheckBox* hideCat;
    QWidget* logPreview;
};

// Fresh settings composition; original values and live-preview handlers stay
// owned by AppearanceWidget. No legacy group-box layout is displayed here.
class AppearanceSections final : public QWidget {
    static QString tr(const char* text) { return QCoreApplication::translate("Pxl::AppearanceSections", text); }
public:
    AppearanceSections(const AppearanceControls& controls, QWidget* parent) : QWidget(parent)
    {
        setObjectName("pxlAppearanceSections");
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(16);
        auto row = [](QWidget* card, const QString& label, QWidget* control, QPushButton* action = nullptr) {
            auto* field = new QWidget(card);
            auto* line = new QHBoxLayout(field);
            line->setContentsMargins(0, 0, 0, 0);
            line->setSpacing(8);
            line->addWidget(control, 1);
            control->show();
            if (action) { line->addWidget(action); action->show(); }
            static_cast<QVBoxLayout*>(card->layout())->addWidget(settingsRow(card, label, field));
        };
        auto* icons = settingsCard(this, tr("Interface assets"), tr("Choose the icon set used throughout the launcher."));
        row(icons, tr("Theme"), appearanceChoice(controls.theme, icons), controls.themeFolder);
        row(icons, tr("Icon set"), appearanceChoice(controls.icons, icons), controls.iconsFolder);
        static_cast<QVBoxLayout*>(icons->layout())->addWidget(controls.reload, 0, Qt::AlignLeft);
        controls.reload->show();
        layout->addWidget(icons);

        auto* log = settingsCard(this, tr("Log appearance"), tr("Font settings apply to game logs. Preview changes below."));
        auto* font = new QWidget(log);
        auto* fontLine = new QHBoxLayout(font);
        fontLine->setContentsMargins(0, 0, 0, 0);
        fontLine->setSpacing(8);
        fontLine->addWidget(controls.logFont, 1);
        controls.logFont->setMinimumWidth(0);
        controls.logFont->show();
        controls.logSize->setFixedWidth(92);
        controls.logSize->setAccessibleName(tr("Log font size"));
        fontLine->addWidget(controls.logSize);
        controls.logSize->show();
        row(log, tr("Font"), font);
        controls.logPreview->setMinimumHeight(110);
        controls.logPreview->setMaximumHeight(150);
        static_cast<QVBoxLayout*>(log->layout())->addWidget(controls.logPreview);
        controls.logPreview->show();
        layout->addWidget(log);

        auto* cat = settingsCard(this, tr("Cat mascot"), tr("Keep your companion visible, or hide its navigation button."));
        auto* catLayout = static_cast<QVBoxLayout*>(cat->layout());
        auto* visible = new ToggleSwitch(tr("Show the cat button"), cat);
        visible->setObjectName("appearanceCatVisible");
        visible->setChecked(!controls.hideCat->isChecked());
        connect(visible, &QCheckBox::toggled, controls.hideCat, [source = controls.hideCat](bool value) { source->setChecked(!value); });
        connect(controls.hideCat, &QCheckBox::toggled, visible, [visible](bool value) { visible->setChecked(!value); });
        catLayout->addWidget(visible);
        row(cat, tr("Cat pack"), appearanceChoice(controls.cats, cat), controls.catsFolder);
        row(cat, tr("Scaling"), appearanceChoice(controls.catScale, cat));
        auto* preview = new QWidget(cat);
        auto* previewLine = new QHBoxLayout(preview);
        previewLine->setContentsMargins(0, 0, 0, 0);
        previewLine->setSpacing(16);
        controls.catPreview->setFixedSize(80, 112);
        controls.catPreview->setIconSize(QSize(64, 96));
        controls.catPreview->setFocusPolicy(Qt::NoFocus);
        controls.catPreview->setAttribute(Qt::WA_TransparentForMouseEvents);
        controls.catPreview->setStyleSheet("QPushButton { background: transparent; border: none; }");
        previewLine->addWidget(controls.catPreview);
        controls.catPreview->show();
        auto* opacity = new QWidget(preview);
        auto* opacityLayout = new QVBoxLayout(opacity);
        opacityLayout->setContentsMargins(0, 0, 0, 0);
        opacityLayout->addWidget(new QLabel(tr("Opacity"), opacity));
        opacityLayout->addWidget(controls.catOpacity);
        controls.catOpacity->setAccessibleName(tr("Cat opacity"));
        controls.catOpacity->show();
        previewLine->addWidget(opacity, 1);
        catLayout->addWidget(preview);
        layout->addWidget(cat);
    }
};
} // namespace Pxl

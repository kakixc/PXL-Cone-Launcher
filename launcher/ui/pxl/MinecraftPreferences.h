// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "SettingsCards.h"
#include "ToggleSwitch.h"
#include "LibraryControls.h"
#include <QScrollArea>
#include <QSpinBox>
#include <QSignalBlocker>

namespace Pxl {
inline QString minecraftPreferenceText(const char* text) { return QCoreApplication::translate("Pxl::MinecraftPreferences",text); }
// Fresh presentation controls; existing MinecraftSettingsWidget remains the settings authority.
class MinecraftPreferences final : public QWidget {
public:
    explicit MinecraftPreferences(QWidget* source,QWidget* parent=nullptr) : QWidget(parent)
    {
        setObjectName("pxlMinecraftPreferences");
        auto* layout=new QVBoxLayout(this); layout->setContentsMargins(0,8,0,0);
        auto* scroll=new QScrollArea(this); scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame);
        scroll->setStyleSheet("QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; border: none; }");
        auto* content=new QWidget(scroll); auto* cards=new QVBoxLayout(content); cards->setContentsMargins(0,0,10,16); cards->setSpacing(14);
        scroll->setWidget(content); layout->addWidget(scroll);
        auto card=[&](const char* title,const char* description) { auto* result=settingsCard(content,minecraftPreferenceText(title),minecraftPreferenceText(description)); cards->addWidget(result); return result; };
        auto toggle=[source](QFrame* target,const char* name) {
            auto* original=source->findChild<QCheckBox*>(name);
            if (!original) return;
            auto* control=new ToggleSwitch(QString(original->text()).remove('&'),target); control->setObjectName("pxl-"+QString::fromLatin1(name));
            control->setChecked(original->isChecked()); control->setEnabled(original->isEnabled());
            QObject::connect(control,&QCheckBox::toggled,original,&QCheckBox::setChecked);
            QObject::connect(original,&QCheckBox::toggled,control,&QCheckBox::setChecked);
            static_cast<QVBoxLayout*>(target->layout())->addWidget(control);
        };
        auto* window=card("Game window","Default size and window behavior when launching Minecraft.");
        toggle(window,"maximizedCheckBox");
        if (auto* maximized=source->findChild<QCheckBox*>("maximizedCheckBox")) {
            auto* warning=new QLabel(minecraftPreferenceText("Maximized launch is simulated and may behave differently between Minecraft versions."),window);
            warning->setWordWrap(true); warning->setProperty("pxlFlowHint",true);
            warning->setVisible(maximized->isChecked());
            QObject::connect(maximized,&QCheckBox::toggled,warning,&QWidget::setVisible);
            static_cast<QVBoxLayout*>(window->layout())->addWidget(warning);
        }
        auto* dimensions=new QWidget(window); auto* sizes=new QHBoxLayout(dimensions); sizes->setContentsMargins(0,0,0,0); sizes->setSpacing(10);
        for (const auto& field : {QPair<const char*,const char*>("windowWidthSpinBox","Width"),QPair<const char*,const char*>("windowHeightSpinBox","Height")}) {
            auto* original=source->findChild<QSpinBox*>(field.first); if (!original) continue;
            auto* input=new QSpinBox(dimensions); input->setObjectName("pxl-"+QString::fromLatin1(field.first)); input->setRange(original->minimum(),original->maximum()); input->setSingleStep(original->singleStep()); input->setValue(original->value());
            input->setButtonSymbols(QAbstractSpinBox::NoButtons); input->setSuffix(" px"); input->setFixedHeight(38); input->setAccessibleName(minecraftPreferenceText(field.second));
            input->setStyleSheet("QSpinBox { background: rgba(43,47,54,190); color: #edf0f4; border: 1px solid rgba(199,210,225,35); border-radius: 10px; padding: 4px 12px; } QSpinBox:focus { border-color: rgba(242,163,101,150); }");
            QObject::connect(input,qOverload<int>(&QSpinBox::valueChanged),original,&QSpinBox::setValue);
            QObject::connect(original,qOverload<int>(&QSpinBox::valueChanged),input,&QSpinBox::setValue);
            sizes->addWidget(new QLabel(minecraftPreferenceText(field.second),dimensions)); sizes->addWidget(input,1);
        }
        static_cast<QVBoxLayout*>(window->layout())->addWidget(dimensions);
        auto* behavior=card("Launcher behavior","Choose what the launcher does while you play.");
        toggle(behavior,"closeAfterLaunchCheck"); toggle(behavior,"quitAfterGameStopCheck");
        auto* logs=card("Game log","Keep launch diagnostics available without opening an extra console every time.");
        toggle(logs,"showConsoleCheck"); toggle(logs,"showConsoleErrorCheck"); toggle(logs,"autoCloseConsoleCheck");
        auto* playtime=card("Playtime","Control the playtime shown on your instances and across the launcher.");
        for (auto name : {"showGameTime","recordGameTime","showGlobalGameTime","showGameTimeWithoutDays"}) toggle(playtime,name);
        if (auto* original=source->findChild<QComboBox*>("applyElyPatch")) {
            auto* ely=card("Ely.by integration","Choose when to apply the existing Ely.by game patch.");
            auto* choice=new ChoiceChip(ely); choice->setObjectName("pxl-applyElyPatch"); choice->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
            const char* choices[]={"Always","Ely.by and offline profiles","Ely.by profiles only","Never"};
            for (int i=0;i<original->count();++i) choice->addItem(i<4 ? minecraftPreferenceText(choices[i]) : original->itemText(i),original->itemData(i));
            choice->setCurrentIndex(original->currentIndex());
            QObject::connect(choice,qOverload<int>(&QComboBox::currentIndexChanged),original,&QComboBox::setCurrentIndex);
            QObject::connect(original,qOverload<int>(&QComboBox::currentIndexChanged),choice,&QComboBox::setCurrentIndex);
            static_cast<QVBoxLayout*>(ely->layout())->addWidget(choice);
        }
        cards->addStretch();
    }
};
} // namespace Pxl

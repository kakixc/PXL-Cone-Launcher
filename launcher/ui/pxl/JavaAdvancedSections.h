// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "LibraryControls.h"
#include "SettingsCards.h"
#include "ToggleSwitch.h"
#include <QPlainTextEdit>
#include <QRadioButton>
#include <QSpinBox>
#include <QSignalBlocker>

namespace Pxl {
struct JavaAdvancedControls {
    QCheckBox *optimized, *skipCompatibility, *skipWizard;
    QList<QRadioButton*> collectors;
    QLineEdit* generatedArgs;
    QPlainTextEdit* customArgs;
    QLabel* notice;
    QSpinBox* permGen;
};
inline QWidget* javaAdvancedSections(const JavaAdvancedControls& source, QWidget* parent)
{
    auto text = [](const char* value) { return QCoreApplication::translate("Pxl::JavaAdvancedSections", value); };
    auto* content = new QWidget(parent);
    content->setObjectName("pxlJavaAdvanced");
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);
    auto card = [&](const char* title, const char* description) {
        auto* result = settingsCard(content, text(title), text(description));
        layout->addWidget(result);
        return result;
    };
    auto toggle = [](QFrame* card, QCheckBox* original, const QString& title) {
        auto* control = new ToggleSwitch(title, card);
        control->setObjectName("pxl-" + original->objectName());
        control->setChecked(original->isChecked());
        control->setEnabled(original->isEnabled());
        QObject::connect(control, &QCheckBox::toggled, original, &QCheckBox::setChecked);
        QObject::connect(original, &QCheckBox::toggled, control, &QCheckBox::setChecked);
        static_cast<QVBoxLayout*>(card->layout())->addWidget(control);
    };
    auto* performance = card("Runtime arguments", "Keep the default profile unless a modpack requires specific Java options.");
    auto* performanceLayout = static_cast<QVBoxLayout*>(performance->layout());
    toggle(performance, source.optimized, text("Use Mojang's recommended arguments"));
    auto* collector = new ChoiceChip(performance);
    collector->setObjectName("pxlCollectorPreset");
    int current = -1;
    for (int i = 0; i < source.collectors.size(); ++i) {
        auto* original = source.collectors[i];
        collector->addItem(QString(original->text()).remove('&'));
        if (original->isChecked()) current = i;
        QObject::connect(original, &QRadioButton::toggled, collector, [collector, i](bool checked) {
            if (checked) { const QSignalBlocker blocker(collector); collector->setCurrentIndex(i); }
        });
    }
    collector->setCurrentIndex(current);
    QObject::connect(collector, &QComboBox::activated, content, [controls = source.collectors](int index) {
        if (index >= 0 && index < controls.size()) controls[index]->click();
    });
    performanceLayout->addWidget(settingsRow(performance, text("Garbage collector"), collector));
    auto* generatedLabel = new QLabel(text("Arguments selected by the launcher"), performance);
    performanceLayout->addWidget(generatedLabel);
    performanceLayout->addWidget(source.generatedArgs);
    source.generatedArgs->show();
    source.notice->setWordWrap(true);
    performanceLayout->addWidget(source.notice);

    auto* custom = card("Custom JVM arguments", "Applied in addition to the launcher arguments. Leave empty to use the selected profile.");
    source.customArgs->setMinimumHeight(88);
    source.customArgs->setMaximumHeight(132);
    source.customArgs->setPlaceholderText("-Dexample=value");
    source.customArgs->setStyleSheet("QPlainTextEdit { background: rgba(17,21,26,110); border: 1px solid rgba(188,200,216,35); border-radius: 10px; padding: 10px; }");
    static_cast<QVBoxLayout*>(custom->layout())->addWidget(source.customArgs);
    source.customArgs->show();

    auto* checks = card("Compatibility and legacy Java", "Disabling compatibility checks can prevent Minecraft from starting. PermGen is only used by old Java runtimes.");
    toggle(checks, source.skipCompatibility, text("Skip Java compatibility checks"));
    toggle(checks, source.skipWizard, text("Skip the Java setup prompt"));
    static_cast<QVBoxLayout*>(checks->layout())->addWidget(settingsRow(checks, text("Legacy PermGen memory"), source.permGen));
    source.permGen->show();
    return content;
}
} // namespace Pxl

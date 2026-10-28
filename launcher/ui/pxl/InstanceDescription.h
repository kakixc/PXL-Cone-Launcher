// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "SettingsCards.h"
#include <BaseInstance.h>
#include <QCoreApplication>
#include <QTextBrowser>
#include <QPlainTextEdit>

namespace Pxl {
class InstanceDescriptionEditor final : public QPlainTextEdit {
public:
    InstanceDescriptionEditor(BaseInstance* instance, QWidget* parent) : QPlainTextEdit(parent), m_instance(instance)
    {
        setObjectName("instanceDescriptionEditor");
        setAccessibleName(QCoreApplication::translate("MinecraftSettingsWidget", "Description"));
        setPlaceholderText(QCoreApplication::translate("MinecraftSettingsWidget", "For example: Survival with friends and mods"));
        setStyleSheet("QPlainTextEdit { color: #e8ebef; background: rgba(24,27,32,160); border: 1px solid #383e47; border-radius: 10px; padding: 12px; } QPlainTextEdit:focus { border-color: #a67a58; }");
        setMinimumHeight(110);
        setMaximumHeight(200);
        loadDescription();
    }
    void loadDescription() { setPlainText(m_instance->description()); }
    void saveDescription() { m_instance->setDescription(toPlainText()); }
private:
    BaseInstance* m_instance;
};

inline QString briefInstanceDescription(const QString& description)
{
    const auto text = description.simplified();
    return text.size() > 240 ? text.left(237) + QStringLiteral("…") : text;
}

// Plain text, bounded height: long descriptions never resize the launcher.
inline QFrame* instanceDescriptionCard(BaseInstance* instance, QWidget* parent)
{
    auto* card = settingsCard(parent, QCoreApplication::translate("MinecraftSettingsWidget", "Description"), {});
    card->setObjectName("instanceDescriptionCard");
    // Keep the shared settings surface selector after assigning a distinct lookup name.
    card->setStyleSheet(card->styleSheet().replace("QFrame#pxlSettingsCard", "QFrame#instanceDescriptionCard"));
    auto* text = new QTextBrowser(card);
    text->setObjectName("instanceDescriptionText");
    text->setOpenLinks(false);
    text->setFrameShape(QFrame::NoFrame);
    text->setMinimumHeight(42);
    text->setMaximumHeight(126);
    text->setStyleSheet("QTextBrowser { background: transparent; border: none; color: #d5dbe4; }");
    static_cast<QVBoxLayout*>(card->layout())->addWidget(text);
    auto update = [instance, card, text] {
        text->setPlainText(instance->description());
        card->setVisible(!instance->description().trimmed().isEmpty());
    };
    QObject::connect(instance, &BaseInstance::propertiesChanged, card, update);
    update();
    return card;
}
}

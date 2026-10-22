// SPDX-License-Identifier: GPL-3.0-only
#include "MinecraftTheme.h"

#include <QFontDatabase>
#include <QObject>

// Explicit initialization keeps the resources linked from Launcher_logic in tests too.
static void initializeMinecraftResources()
{
    static const bool initialized = [] {
        Q_INIT_RESOURCE(fonts);
        return true;
    }();
    Q_UNUSED(initialized);
}

QString MinecraftTheme::name()
{
    return QObject::tr("Minecraft");
}

QString MinecraftTheme::tooltip()
{
    return QObject::tr("Blocky controls and pixel lettering inspired by the Minecraft Launcher.");
}

QPalette MinecraftTheme::colorScheme()
{
    auto palette = DarkTheme::colorScheme();
    palette.setColor(QPalette::Window, QColor("#242322"));
    palette.setColor(QPalette::Base, QColor("#191918"));
    palette.setColor(QPalette::AlternateBase, QColor("#30302e"));
    palette.setColor(QPalette::Button, QColor("#484846"));
    palette.setColor(QPalette::Mid, QColor("#aaaaa4"));
    palette.setColor(QPalette::Highlight, QColor("#3c8527"));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Link, QColor("#a0e081"));
    return fadeInactive(palette, fadeAmount(), QColor("#242322"));
}

QString MinecraftTheme::appStyleSheet()
{
    initializeMinecraftResources();
    static const int fontId = QFontDatabase::addApplicationFont(":/fonts/Monocraft.ttf");
    const auto families = QFontDatabase::applicationFontFamilies(fontId);
    const QString family = families.isEmpty() ? QString() : families.first();
    QString sheet = ConeTheme::appStyleSheet() + QStringLiteral(R"(
        QPushButton, QToolButton {
            color: #ffffff; background: #484846;
            border: 2px solid #101010; border-top: 3px solid #777773;
            border-bottom: 6px solid #171716; border-radius: 0px;
            padding: 6px 12px;
        }
        QPushButton:hover, QToolButton:hover { background: #60605b; border-top-color: #99998f; }
        QPushButton:pressed, QToolButton:pressed, QPushButton:checked, QToolButton:checked {
            background: #363633; border-top: 6px solid #171716; border-bottom: 3px solid #777773;
        }
        QPushButton#workspaceContinue, QPushButton#workspacePlaySelected {
            background: #3c8527; border-top-color: #84c65c; border-bottom-color: #1e4214;
            font-size: 16px; padding: 10px 18px;
        }
        QPushButton#workspaceContinue:hover, QPushButton#workspacePlaySelected:hover { background: #4c9e31; }
        QPushButton#workspaceContinue:pressed, QPushButton#workspacePlaySelected:pressed {
            background: #326f20; border-top-color: #1e4214; border-bottom-color: #84c65c;
        }
        QPushButton:focus, QToolButton:focus { border-left: 2px solid #ffffff; border-right: 2px solid #ffffff; }
        QPushButton:disabled, QToolButton:disabled {
            color: #a3a39d; background: #343431; border-top-color: #50504b; border-bottom-color: #20201e;
        }
        QPushButton#workspaceContinue:disabled, QPushButton#workspacePlaySelected:disabled {
            color: #a3a39d; background: #343431; border-top-color: #50504b; border-bottom-color: #20201e;
        }
        QLineEdit, QComboBox { border: 2px solid #101010; border-radius: 0px; padding: 5px; }
        QLineEdit:focus, QComboBox:focus { border-color: #84c65c; }
        QToolTip { color: #ffffff; background: #242322; border: 2px solid #777773; }
    )");
    if (!family.isEmpty()) {
        sheet += QStringLiteral("QPushButton, QToolButton, QLabel[pxlHeading=\"true\"] { font-family: \"%1\"; }").arg(family);
    }
    return sheet;
}

// SPDX-License-Identifier: GPL-3.0-only
#include "ConeTheme.h"
#include <QObject>

QString ConeTheme::name() { return QObject::tr("Cone"); }
QString ConeTheme::tooltip() { return QObject::tr("The standard PXL Cone appearance."); }

QPalette ConeTheme::colorScheme()
{
    auto p = DarkTheme::colorScheme();
    p.setColor(QPalette::Window, QColor("#17191c"));
    p.setColor(QPalette::WindowText, QColor("#f0f1f3"));
    p.setColor(QPalette::Base, QColor("#202328"));
    p.setColor(QPalette::AlternateBase, QColor("#272b31"));
    p.setColor(QPalette::Text, QColor("#f0f1f3"));
    p.setColor(QPalette::Button, QColor("#30353d"));
    p.setColor(QPalette::ButtonText, QColor("#f0f1f3"));
    p.setColor(QPalette::Mid, QColor("#a4adba"));
    p.setColor(QPalette::Dark, QColor("#363c45"));
    p.setColor(QPalette::Highlight, QColor("#f2a365"));
    p.setColor(QPalette::HighlightedText, QColor("#201a15"));
    p.setColor(QPalette::Link, QColor("#f2a365"));
    p.setColor(QPalette::PlaceholderText, QColor("#8993a2"));
    return fadeInactive(p, 0.55, QColor("#17191c"));
}

QString ConeTheme::appStyleSheet()
{
    return QStringLiteral(R"(
        QWidget { font-family: "Segoe UI", "Noto Sans", "DejaVu Sans", sans-serif; font-size: 13px; }
        QPushButton, QToolButton {
            background: palette(button); color: palette(button-text);
            border: 1px solid palette(dark); border-radius: 8px; padding: 8px 14px;
        }
        QPushButton:hover, QToolButton:hover { border-color: palette(mid); }
        QPushButton:pressed, QToolButton:pressed { background: palette(base); }
        QPushButton:focus, QToolButton:focus { border: 1px solid palette(highlight); }
        QPushButton:disabled, QToolButton:disabled { color: palette(mid); background: palette(window); }
        QPushButton[pxlPrimary="true"] {
            background: palette(highlight); color: palette(highlighted-text); border-color: palette(highlight); font-weight: 600;
        }
        QPushButton[pxlPrimary="true"]:hover { background: #ffc08d; }
        QPushButton[pxlPrimary="true"]:disabled { background: palette(base); color: palette(mid); border-color: palette(dark); }
        QPushButton[pxlNav="true"] { border: 1px solid transparent; background: transparent; padding: 0px; border-radius: 12px; }
        QPushButton[pxlNav="true"]:hover { background: palette(alternate-base); }
        QPushButton[pxlNav="true"]:checked { background: palette(button); border-color: palette(highlight); }
        QPushButton[pxlNav="true"]:focus { border-color: palette(highlight); }
        QLineEdit, QComboBox { background: palette(base); border: 1px solid palette(dark); border-radius: 8px; padding: 9px 12px; }
        QLineEdit:focus, QComboBox:focus { border-color: palette(highlight); }
        QMenu { background: palette(alternate-base); padding: 5px; border: 1px solid palette(dark); }
        QMenu::item { padding: 7px 22px; border-radius: 4px; }
        QMenu::item:selected { background: palette(highlight); color: palette(highlighted-text); }
        QScrollBar:vertical { background: transparent; width: 8px; margin: 0px; }
        QScrollBar::handle:vertical { background: palette(dark); min-height: 32px; border-radius: 4px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
        QToolTip { color: palette(text); background: palette(alternate-base); border: 1px solid palette(dark); padding: 6px; }
    )");
}

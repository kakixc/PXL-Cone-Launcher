// SPDX-License-Identifier: GPL-3.0-only
#include "ConeTheme.h"
#include <QObject>
#include <QFontDatabase>
#include <QApplication>
#include <QEvent>
#include <QPainterPath>
#include <QWidget>

namespace {
class RoundedTooltipWindows final : public QObject {
public:
    explicit RoundedTooltipWindows(QObject* parent) : QObject(parent) {}
protected:
    bool eventFilter(QObject* object, QEvent* event) override
    {
        if (event->type() == QEvent::Show || event->type() == QEvent::Resize) {
            auto* widget = qobject_cast<QWidget*>(object);
            if (widget && widget->inherits("QTipLabel")) {
                QPainterPath outline;
                outline.addRoundedRect(QRectF(widget->rect()), 9, 9);
                widget->setMask(QRegion(outline.toFillPolygon().toPolygon()));
            }
        }
        return false;
    }
};
}

static void initializeConeFonts()
{
    Q_INIT_RESOURCE(fonts);
}

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
    static const auto tooltips = [] {
        auto* filter = new RoundedTooltipWindows(qApp);
        qApp->installEventFilter(filter);
        return filter;
    }();
    Q_UNUSED(tooltips);
    static const int fontId = [] { initializeConeFonts(); return QFontDatabase::addApplicationFont(":/fonts/Inter.ttf"); }();
    Q_UNUSED(fontId);
    return QStringLiteral(R"(
        QWidget { font-family: "Inter", "Segoe UI", "Noto Sans", sans-serif; font-size: 14px; font-weight: 500; }
        QPushButton, QToolButton {
            background: rgba(255, 238, 218, 15); color: palette(button-text);
            border: 1px solid transparent; border-radius: 10px; padding: 8px 14px; font-weight: 600;
        }
        QPushButton:hover, QToolButton:hover { background: rgba(255, 238, 218, 28); }
        QPushButton:pressed, QToolButton:pressed { background: rgba(255, 238, 218, 10); }
        QPushButton:focus, QToolButton:focus { border: 1px solid palette(mid); }
        QPushButton:disabled, QToolButton:disabled { color: palette(mid); background: palette(window); }
        QPushButton[pxlPrimary="true"] {
            background: palette(highlight); color: palette(highlighted-text); border-color: palette(highlight); font-weight: 600;
        }
        QPushButton[pxlPrimary="true"]:hover { background: #ffc08d; }
        QPushButton[pxlPrimary="true"]:disabled { background: palette(base); color: palette(mid); border-color: palette(dark); }
        QPushButton[pxlStop="true"] { background: #673b3c; color: #ffe8e8; border-color: transparent; }
        QPushButton[pxlStop="true"]:hover { background: #805052; }
        QPushButton[pxlMode="true"]:checked, QRadioButton[pxlChoice="true"]:checked {
            background: rgba(242, 163, 101, 30); color: #ffc08e; border: 1px solid rgba(242, 163, 101, 90); border-radius: 9px;
        }
        QRadioButton[pxlChoice="true"] {
            background: rgba(255, 238, 218, 15); padding: 9px 12px; border: 1px solid rgba(255, 238, 218, 35);
            border-radius: 9px; font-weight: 600; color: palette(text);
        }
        QRadioButton[pxlChoice="true"]:hover { background: rgba(255, 238, 218, 30); border-color: rgba(255, 238, 218, 65); }
        QRadioButton[pxlChoice="true"]:checked:hover { background: rgba(242, 163, 101, 45); border-color: rgba(242, 163, 101, 130); }
        QRadioButton[pxlChoice="true"]:focus { border-color: palette(mid); }
        QRadioButton[pxlChoice="true"]::indicator { width: 0px; height: 0px; }
        QPushButton[pxlQuiet="true"] { background: transparent; border-color: transparent; color: palette(mid); }
        QPushButton[pxlQuiet="true"]:hover { color: palette(text); border-color: palette(dark); }
        QPushButton[pxlNav="true"] { border: 1px solid transparent; background: transparent; padding: 8px 12px; border-radius: 6px; text-align: left; }
        QPushButton[pxlNav="true"]:hover { background: palette(alternate-base); }
        QPushButton[pxlNav="true"]:checked { background: palette(button); border-left: 3px solid palette(highlight); font-weight: 600; }
        QPushButton[pxlNav="true"]:focus { border-color: palette(highlight); }
        QPushButton[pxlNav="true"][pxlBusy="true"] { background: rgba(242, 163, 101, 30); }
        QLineEdit, QComboBox { background: palette(base); border: 1px solid palette(dark); border-radius: 8px; padding: 9px 12px; }
        QLineEdit:focus, QComboBox:focus { border-color: palette(highlight); }
        QComboBox { padding-right: 32px; combobox-popup: 0; }
        QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: top right; width: 30px; border: none; }
        QComboBox::down-arrow { image: url(:/pxl/icons/caret-down.svg); width: 14px; height: 14px; }
        QComboBox QAbstractItemView {
            background: palette(base); color: palette(text); selection-background-color: #454c58;
            selection-color: #ffffff; border: 1px solid palette(dark); padding: 4px; border-radius: 8px; outline: none;
        }
        QComboBox QAbstractItemView::item { min-height: 30px; padding: 3px 8px; border: none; border-radius: 5px; }
        QComboBox QAbstractItemView::item:hover, QComboBox QAbstractItemView::item:selected {
            background: #454c58; color: #ffffff; border: none; border-radius: 5px;
        }
        QMenu { background: palette(base); padding: 6px; border: 1px solid palette(dark); border-radius: 10px; }
        QMenu::item { padding: 9px 22px; border-radius: 6px; }
        QMenu::item:selected { background: palette(button); color: palette(text); }
        QMenu::separator { height: 1px; background: palette(dark); margin: 5px 8px; }
        QDialog[pxlFlow="true"] { background: palette(window); }
        QLabel[pxlFlowTitle="true"] { font-size: 24px; font-weight: 700; }
        QLabel[pxlFlowHint="true"] { color: palette(mid); }
        PageContainer[pxlFlow="true"] QListView#conePageNavigation {
            background: palette(base); border: none; border-radius: 12px; padding: 8px;
        }
        PageContainer[pxlFlow="true"] QListView#conePageNavigation::item { min-height: 38px; padding: 5px 10px; margin: 2px 0; border-radius: 8px; }
        PageContainer[pxlFlow="true"] QListView#conePageNavigation::item:selected { background: palette(button); color: palette(text); }
        PageContainer[pxlFlow="true"] QListView#conePageNavigation::item:hover { background: palette(alternate-base); }
        PageContainer[pxlFlow="true"] QLabel#conePageHeading { font-size: 18px; font-weight: 700; padding: 4px 0; }
        QWidget[pxlFlow="true"] QTreeView, QWidget[pxlFlow="true"] QListView#packView,
        QWidget[pxlFlow="true"] QTextBrowser {
            background: palette(base); alternate-background-color: palette(base); border: none; border-radius: 10px;
            selection-background-color: palette(button); selection-color: palette(text);
        }
        QWidget[pxlFlow="true"] QTreeView::item { padding: 6px; min-height: 24px; }
        QWidget[pxlFlow="true"] QHeaderView::section { background: palette(base); color: palette(mid); border: none; padding: 8px; }
        QWidget[pxlFlow="true"] QScrollArea { border: none; }
        QDialog[pxlFlow="true"] QProgressBar { background: palette(base); border: none; border-radius: 5px; min-height: 10px; max-height: 10px; }
        QDialog[pxlFlow="true"] QProgressBar::chunk { background: palette(highlight); border-radius: 5px; }
        QScrollBar:vertical { background: transparent; width: 8px; margin: 0px; }
        QScrollBar::handle:vertical { background: palette(dark); min-height: 32px; border-radius: 4px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
        QToolTip { color: #f0f1f3; background: #2b2e34; border: 1px solid #44474e; border-radius: 8px; padding: 5px 8px; font-size: 12px; font-weight: 500; }
    )");
}

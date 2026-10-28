// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QMouseEvent>
#include <QCoreApplication>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QTextDocumentFragment>
#include <QTextLayout>
#include <functional>
#include "modplatform/ModIndex.h"
#include "ui/widgets/ProjectItem.h"

namespace Pxl {
class CatalogCardDelegate final : public QStyledItemDelegate {
public:
    static QString label(const char* source) { return QCoreApplication::translate("Pxl::CatalogCardDelegate", source); }
    CatalogCardDelegate(QObject* parent, std::function<void(QModelIndex)> select, std::function<void(QModelIndex)> details)
        : QStyledItemDelegate(parent), m_select(std::move(select)), m_details(std::move(details)) {}
    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override { return {320, 136}; }
    static QRect actionRect(const QRect& row) { return {row.right() - 126, row.top() + 18, 108, 34}; }
    static QRect detailsRect(const QRect& row) { return {row.right() - 126, row.top() + 61, 108, 28}; }
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const auto area = option.rect.adjusted(1, 2, -1, -4);
        const bool selected = index.data(Qt::CheckStateRole).toInt() == Qt::Checked;
        const bool installed = index.data(UserDataTypes::INSTALLED).toBool() && !selected;
        const bool hover = option.state.testFlag(QStyle::State_MouseOver);
        painter->setBrush(hover ? QColor("#30343c") : QColor("#22262c"));
        painter->setPen(selected ? QColor(242, 163, 101, 150) : QColor("#363c44"));
        painter->drawRoundedRect(area, 14, 14);
        const QRect iconArea(area.left() + 18, area.top() + 20, 64, 64);
        painter->setBrush(QColor("#30353d"));
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(iconArea, 12, 12);
        QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        if (icon.isNull()) icon = QIcon(":/pxl/icons/squares-four.svg");
        icon.paint(painter, iconArea.adjusted(5, 5, -5, -5));
        const int left = iconArea.right() + 18;
        const int width = qMax(20, area.width() - (left - area.left()) - 150);
        auto pack = index.data(Qt::UserRole).value<ModPlatform::IndexedPack::Ptr>();
        const QString name = pack ? pack->name : index.data(UserDataTypes::TITLE).toString();
        QFont titleFont = option.font;
        titleFont.setPixelSize(16);
        titleFont.setWeight(QFont::DemiBold);
        painter->setFont(titleFont);
        painter->setPen(QColor("#f0f2f5"));
        painter->drawText(QRect(left, area.top() + 16, width, 24), Qt::AlignVCenter,
                          QFontMetrics(titleFont).elidedText(name, Qt::ElideRight, width));
        QFont textFont = option.font;
        textFont.setPixelSize(13);
        textFont.setWeight(QFont::Medium);
        painter->setFont(textFont);
        painter->setPen(QColor("#a9b3c1"));
        QStringList authors;
        if (pack) for (const auto& author : pack->authors) authors.append(author.name);
        painter->drawText(QRect(left, area.top() + 43, width, 20), Qt::AlignVCenter,
                          QFontMetrics(textFont).elidedText(authors.join(", "), Qt::ElideRight, width));
        auto description = QTextDocumentFragment::fromHtml(index.data(UserDataTypes::DESCRIPTION).toString()).toPlainText().simplified();
        QTextLayout text(description, textFont);
        text.beginLayout();
        for (int row = 0; row < 2; ++row) {
            auto line = text.createLine();
            if (!line.isValid()) break;
            line.setLineWidth(width);
            line.setPosition(QPointF(0, row * 19));
        }
        text.endLayout();
        text.draw(painter, QPointF(left, area.top() + 71));
        const auto action = actionRect(option.rect);
        painter->setPen(Qt::NoPen);
        painter->setBrush(installed ? QColor(143, 164, 155, 26) : selected ? QColor(242, 163, 101, 36) : QColor("#f2a365"));
        painter->drawRoundedRect(action, 9, 9);
        painter->setPen(installed ? QColor("#aebbaa") : selected ? QColor("#f2b27f") : QColor("#231a13"));
        painter->drawText(action, Qt::AlignCenter, installed ? label("Installed") : selected ? label("Added") : label("Add"));
        painter->restore();
    }
    bool editorEvent(QEvent* event, QAbstractItemModel*, const QStyleOptionViewItem& option, const QModelIndex& index) override
    {
        if (event->type() != QEvent::MouseButtonRelease && event->type() != QEvent::MouseButtonPress &&
            event->type() != QEvent::MouseButtonDblClick) return false;
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() != Qt::LeftButton) return false;
        const bool action = actionRect(option.rect).contains(mouse->pos());
        if (!option.rect.contains(mouse->pos())) return false;
        if (event->type() == QEvent::MouseButtonRelease) {
            if (action) {
                if (!index.data(UserDataTypes::INSTALLED).toBool() || index.data(Qt::CheckStateRole).toInt() == Qt::Checked) m_select(index);
            }
            else m_details(index);
        }
        return true;
    }
private:
    std::function<void(QModelIndex)> m_select;
    std::function<void(QModelIndex)> m_details;
};
} // namespace Pxl

// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "Backdrop.h"
#include "CheckListDelegate.h"
#include <QLabel>
#include <QHBoxLayout>
#include <QPainterPath>

namespace Pxl {
class ReviewInstanceCard final : public QWidget {
public:
    ReviewInstanceCard(const QString& name, const QIcon& icon, const QString& cover, QWidget* parent)
        : QWidget(parent), m_backdrop(blurredBackdrop(cover))
    {
        setObjectName("reviewInstanceCard");
        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(18, 18, 18, 18);
        layout->setSpacing(16);
        auto* artwork = new QLabel(this);
        artwork->setPixmap(icon.pixmap(56, 56));
        artwork->setFixedSize(56, 56);
        layout->addWidget(artwork);
        auto* title = new QLabel(name, this);
        title->setTextFormat(Qt::PlainText);
        title->setWordWrap(true);
        title->setStyleSheet("font-size: 20px; font-weight: 700; background: transparent;");
        layout->addWidget(title, 1);
        setMinimumHeight(96);
    }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        QPainterPath shape;
        shape.addRoundedRect(rect(), 14, 14);
        painter.setClipPath(shape);
        paintBackdrop(painter, rect(), m_backdrop, 130);
    }
private:
    QPixmap m_backdrop;
};

// The upstream tree remains the selection/metadata authority; only its presentation changes.
class ResourceReviewDelegate final : public QStyledItemDelegate {
public:
    explicit ResourceReviewDelegate(QObject* parent) : QStyledItemDelegate(parent) {}
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        return index.parent().isValid() ? QStyledItemDelegate::sizeHint(option, index) : QSize(400, 80);
    }
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        if (index.parent().isValid()) { QStyledItemDelegate::paint(painter, option, index); return; }
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const auto area = option.rect.adjusted(1, 4, -2, -4);
        const bool checked = index.data(Qt::CheckStateRole).toInt() == Qt::Checked;
        painter->setBrush(QColor(255, 255, 255, option.state.testFlag(QStyle::State_MouseOver) ? 16 : 7));
        painter->setPen(QColor(242, 163, 101, checked ? 85 : 20));
        painter->drawRoundedRect(area, 12, 12);
        const QRect box(area.left() + 14, area.center().y() - 10, 20, 20);
        painter->setBrush(checked ? QColor("#f2a365") : QColor("#20242a"));
        painter->setPen(QColor("#8d99a8"));
        painter->drawRoundedRect(box, 5, 5);
        if (checked) {
            painter->setPen(QPen(QColor("#2d2017"), 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter->drawLine(box.topLeft() + QPoint(5, 10), box.topLeft() + QPoint(9, 14));
            painter->drawLine(box.topLeft() + QPoint(9, 14), box.topLeft() + QPoint(16, 6));
        }
        auto icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        if (icon.isNull()) icon = QIcon(":/pxl/icons/squares-four.svg");
        icon.paint(painter, QRect(area.left() + 48, area.center().y() - 24, 48, 48));
        const int left = area.left() + 112;
        const int width = qMax(0, area.right() - left - 14);
        auto font = option.font;
        font.setPixelSize(16);
        font.setWeight(QFont::DemiBold);
        painter->setFont(font);
        painter->setPen(QColor("#eef0f4"));
        painter->drawText(QRect(left, area.top() + 12, width, 24), Qt::AlignVCenter,
                          QFontMetrics(font).elidedText(index.data().toString(), Qt::ElideRight, width));
        font.setPixelSize(12);
        font.setWeight(QFont::Normal);
        painter->setFont(font);
        painter->setPen(QColor("#aab5c4"));
        painter->drawText(QRect(left, area.top() + 40, width, 20), Qt::AlignVCenter,
                          QFontMetrics(font).elidedText(index.data(Qt::UserRole + 1).toString(), Qt::ElideRight, width));
        if (option.state.testFlag(QStyle::State_HasFocus)) {
            painter->setBrush(Qt::NoBrush);
            painter->setPen(QColor("#e8bd9b"));
            painter->drawRoundedRect(area.adjusted(2, 2, -2, -2), 10, 10);
        }
        painter->restore();
    }
    bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) override
    {
        return index.parent().isValid() ? QStyledItemDelegate::editorEvent(event, model, option, index)
                                       : m_checks.editorEvent(event, model, option, index);
    }
private:
    CheckListDelegate m_checks;
};
} // namespace Pxl

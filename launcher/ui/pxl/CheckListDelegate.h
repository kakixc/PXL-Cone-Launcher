// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QStyledItemDelegate>
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>

namespace Pxl {
// Checked state is the only selection state; the whole row is a toggle target.
class CheckListDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem&,const QModelIndex&) const override { return {300,60}; }
    void paint(QPainter* p,const QStyleOptionViewItem& option,const QModelIndex& index) const override
    {
        p->save(); p->setRenderHint(QPainter::Antialiasing);
        const auto row=option.rect.adjusted(2,3,-2,-3);
        const bool checked=index.data(Qt::CheckStateRole).toInt()==Qt::Checked;
        const bool enabled=index.flags().testFlag(Qt::ItemIsEnabled);
        const bool hover=option.state.testFlag(QStyle::State_MouseOver);
        p->setBrush(checked ? QColor(242,163,101,25) : QColor(255,255,255,hover ? 15 : 5));
        p->setPen(checked ? QColor(242,163,101,100) : QColor(180,193,210,22));
        p->drawRoundedRect(row,10,10);
        const QRect box(row.left()+14,row.center().y()-10,20,20);
        p->setBrush(checked ? QColor("#f2a365") : QColor(22,26,32,180));
        p->setPen(QPen(enabled ? QColor("#a7b0bc") : QColor("#50565f"),1.3));
        p->drawRoundedRect(box,5,5);
        if (checked) {
            p->setPen(QPen(QColor("#251d17"),2.2,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
            p->drawLine(box.left()+5,box.top()+10,box.left()+9,box.top()+14);
            p->drawLine(box.left()+9,box.top()+14,box.left()+16,box.top()+6);
        }
        qvariant_cast<QIcon>(index.data(Qt::DecorationRole)).paint(p,QRect(row.left()+47,row.center().y()-16,32,32));
        QFont font=option.font; font.setPixelSize(14); font.setWeight(QFont::DemiBold); p->setFont(font);
        p->setPen(enabled ? QColor("#edf0f4") : QColor("#88919d"));
        const auto text=row.adjusted(92,0,-14,0);
        p->drawText(text,Qt::AlignVCenter,QFontMetrics(font).elidedText(index.data().toString(),Qt::ElideRight,text.width()));
        if (option.state.testFlag(QStyle::State_HasFocus)) {
            p->setBrush(Qt::NoBrush); p->setPen(QPen(QColor(220,228,239,115),1)); p->drawRoundedRect(row.adjusted(2,2,-2,-2),8,8);
        }
        p->restore();
    }
    bool editorEvent(QEvent* event,QAbstractItemModel* model,const QStyleOptionViewItem&,const QModelIndex& index) override
    {
        if (!index.flags().testFlag(Qt::ItemIsEnabled) || !index.flags().testFlag(Qt::ItemIsUserCheckable)) return false;
        bool toggle=false;
        if (event->type()==QEvent::MouseButtonPress || event->type()==QEvent::MouseButtonRelease || event->type()==QEvent::MouseButtonDblClick) {
            if (static_cast<QMouseEvent*>(event)->button()!=Qt::LeftButton) return false;
            if (event->type()==QEvent::MouseButtonDblClick) { m_skipRelease=true; return true; }
            if (event->type()==QEvent::MouseButtonRelease) { toggle=!m_skipRelease; m_skipRelease=false; }
        } else if (event->type()==QEvent::KeyPress && static_cast<QKeyEvent*>(event)->key()==Qt::Key_Space) toggle=true;
        else return false;
        if (toggle) model->setData(index,index.data(Qt::CheckStateRole).toInt()==Qt::Checked ? Qt::Unchecked : Qt::Checked,Qt::CheckStateRole);
        return true;
    }
private:
    bool m_skipRelease=false;
};
} // namespace Pxl

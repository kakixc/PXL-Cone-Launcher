// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "LibraryControls.h"
#include <BaseVersionList.h>
#include <QHeaderView>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QTreeView>
#include <QVBoxLayout>

namespace Pxl {
// Presentation only: the existing version model, selection and actions remain in use.
class ManagedJavaDelegate final : public QStyledItemDelegate {
public:
    explicit ManagedJavaDelegate(QObject* parent) : QStyledItemDelegate(parent) {}
    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override { return {320, 98}; }
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        if (index.column() != 0) return;
        painter->save(); painter->setRenderHint(QPainter::Antialiasing);
        const auto card = option.rect.adjusted(1, 4, -3, -4);
        const bool selected = option.state.testFlag(QStyle::State_Selected);
        const bool hovered = option.state.testFlag(QStyle::State_MouseOver);
        painter->setPen(QPen(selected ? QColor(242,163,101,125) : QColor(199,207,219,30),1));
        painter->setBrush(QColor(32,36,42, hovered ? 235 : 200)); painter->drawRoundedRect(card,12,12);
        const QRect icon(card.left()+16,card.top()+19,40,40);
        QIcon(":/pxl/icons/sliders-horizontal.svg").paint(painter,icon.adjusted(7,7,-7,-7));
        const int left=icon.right()+16;
        const int available=qMax(1,card.right()-left-18);
        auto font=option.font; font.setWeight(QFont::DemiBold); painter->setFont(font); painter->setPen(QColor("#edf0f4"));
        const auto version=index.data(BaseVersionList::VersionRole).toString();
        const auto arch=index.data(BaseVersionList::CPUArchitectureRole).toString();
        const auto title=QString("Java %1  ·  %2").arg(version,arch);
        painter->drawText(QRect(left,card.top()+15,available,25),Qt::AlignVCenter,
                          QFontMetrics(font).elidedText(title,Qt::ElideRight,available));
        font.setWeight(QFont::Normal); font.setPointSizeF(qMax(9.0,font.pointSizeF()-1)); painter->setFont(font);
        painter->setPen(QColor("#aeb8c6"));
        const auto path=index.data(BaseVersionList::PathRole).toString();
        painter->drawText(QRect(left,card.top()+45,available,25),Qt::AlignVCenter,
                          QFontMetrics(font).elidedText(path,Qt::ElideMiddle,available));
        painter->restore();
    }
};

inline void styleManagedJavaList(QTreeView* view, QLineEdit* search)
{
    view->setItemDelegate(new ManagedJavaDelegate(view));
    view->setHeaderHidden(true); view->setRootIsDecorated(false);
    view->setAlternatingRowColors(false); view->setMouseTracking(true);
    view->setFrameShape(QFrame::NoFrame); view->setUniformRowHeights(true);
    view->setStyleSheet("QTreeView { background: transparent; border: none; outline: none; }"
                       "QTreeView::item { padding: 0; margin: 0; border: none; background: transparent; }");
    view->viewport()->setAutoFillBackground(false);
    auto columns=[view] {
        for (int i=1;i<view->model()->columnCount();++i) view->hideColumn(i);
        view->header()->setSectionResizeMode(0,QHeaderView::Stretch);
    };
    columns();
    QObject::connect(view->model(),&QAbstractItemModel::modelReset,view,columns);
    if (search) {
        applySearchSurface(search);
        search->addAction(QIcon(":/pxl/icons/magnifying-glass.svg"),QLineEdit::LeadingPosition);
        auto* layout=qobject_cast<QVBoxLayout*>(search->parentWidget()->layout());
        if (layout) { layout->removeWidget(search); layout->insertWidget(0,search); layout->setSpacing(12); }
    }
}
}

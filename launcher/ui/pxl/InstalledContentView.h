// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "LibraryControls.h"
#include "ActionMenu.h"
#include <QAbstractItemModel>
#include <QSortFilterProxyModel>
#include <QItemSelectionModel>
#include <QLabel>
#include <QListView>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QRegularExpression>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QTimer>
#include <functional>

namespace Pxl {
inline QString installedText(const char* text) { return QCoreApplication::translate("Pxl::InstalledContentView",text); }
inline QRect installedToggle(const QRect& row) { return {row.right()-104,row.center().y()-11,40,22}; }
inline QRect installedMore(const QRect& row) { return {row.right()-48,row.center().y()-16,32,32}; }

class InstalledContentDelegate final : public QStyledItemDelegate {
public:
    InstalledContentDelegate(const QStringList& columns,QObject* parent) : QStyledItemDelegate(parent),m_columns(columns) {}
    QSize sizeHint(const QStyleOptionViewItem&,const QModelIndex&) const override { return {300,78}; }
    bool editorEvent(QEvent*,QAbstractItemModel*,const QStyleOptionViewItem&,const QModelIndex&) override { return false; }
    void paint(QPainter* p,const QStyleOptionViewItem& option,const QModelIndex& index) const override
    {
        p->save(); p->setRenderHint(QPainter::Antialiasing); p->setRenderHint(QPainter::SmoothPixmapTransform);
        const auto area=option.rect.adjusted(1,3,-1,-3);
        p->setBrush(QColor(31,35,41,190));
        p->setPen(option.state.testFlag(QStyle::State_Selected) ? QColor(242,163,101,110) : QColor(199,210,225,24));
        p->drawRoundedRect(area,13,13);
        auto data=[&](const QString& column,int role=Qt::DisplayRole) { const int c=m_columns.indexOf(column); return c>=0 ? index.siblingAtColumn(c).data(role) : QVariant(); };
        const auto image=qvariant_cast<QPixmap>(data("Image",Qt::DecorationRole));
        const QRect artwork(area.left()+14,area.center().y()-23,46,46);
        if (!image.isNull()) p->drawPixmap(artwork,image);
        else QIcon(":/pxl/icons/squares-four.svg").paint(p,artwork.adjusted(8,8,-8,-8));
        const int left=artwork.right()+14;
        const int available=qMax(10,installedToggle(option.rect).left()-left-16);
        auto font=option.font; font.setPixelSize(14); font.setWeight(QFont::DemiBold); p->setFont(font); p->setPen(QColor("#edf0f4"));
        p->drawText(QRect(left,area.top()+12,available,22),Qt::AlignVCenter,QFontMetrics(font).elidedText(data("Name").toString(),Qt::ElideRight,available));
        QStringList detail;
        for (const auto& column : {QStringLiteral("Version"),QStringLiteral("Pack Format"),QStringLiteral("File Name")}) {
            const auto value=data(column).toString(); if (!value.isEmpty()) detail.append(value);
        }
        font.setPixelSize(12); font.setWeight(QFont::Normal); p->setFont(font); p->setPen(QColor("#aab5c4"));
        p->drawText(QRect(left,area.top()+37,available,20),Qt::AlignVCenter,QFontMetrics(font).elidedText(detail.join(QStringLiteral(" · ")),Qt::ElideMiddle,available));
        const bool enabled=index.siblingAtColumn(0).data(Qt::CheckStateRole).toInt()==Qt::Checked;
        const auto toggle=installedToggle(option.rect);
        p->setPen(Qt::NoPen); p->setBrush(enabled ? QColor("#d29463") : QColor("#535d69")); p->drawRoundedRect(toggle,11,11);
        p->setBrush(enabled ? QColor("#251c15") : QColor("#d5dce5"));
        p->drawEllipse(QRect(enabled ? toggle.right()-18 : toggle.left()+3,toggle.top()+3,16,16));
        QIcon(":/pxl/icons/dots-three-vertical.svg").paint(p,installedMore(option.rect).adjusted(6,6,-6,-6));
        p->restore();
    }
private:
    QStringList m_columns;
};

class InstalledContentList final : public QListView {
public:
    using QListView::QListView;
    std::function<void(const QModelIndex&,const QPoint&)> showActions;
    std::function<void()> removeSelected;
protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        const auto index=indexAt(event->position().toPoint());
        m_pressed=QPersistentModelIndex(); m_target=0;
        if (event->button()==Qt::LeftButton && index.isValid()) {
            const auto point=event->position().toPoint();
            if (installedToggle(visualRect(index)).contains(point)) m_target=1;
            else if (installedMore(visualRect(index)).contains(point)) m_target=2;
            if (m_target) { m_pressed=index; event->accept(); return; }
        }
        QListView::mousePressEvent(event);
    }
    void mouseReleaseEvent(QMouseEvent* event) override
    {
        const auto index=indexAt(event->position().toPoint());
        const auto pressed=m_pressed; const int target=m_target; m_pressed=QPersistentModelIndex(); m_target=0;
        if (event->button()==Qt::LeftButton && index.isValid() && pressed==index) {
            if (target==1 && installedToggle(visualRect(index)).contains(event->position().toPoint())) {
                const auto active=index.siblingAtColumn(0);
                if (active.flags().testFlag(Qt::ItemIsEnabled) && active.flags().testFlag(Qt::ItemIsUserCheckable))
                    model()->setData(active,active.data(Qt::CheckStateRole).toInt()==Qt::Checked ? Qt::Unchecked : Qt::Checked,Qt::CheckStateRole);
                event->accept(); return;
            }
            if (target==2 && installedMore(visualRect(index)).contains(event->position().toPoint())) {
                if (showActions) showActions(index,viewport()->mapToGlobal(event->position().toPoint()));
                event->accept(); return;
            }
        }
        QListView::mouseReleaseEvent(event);
    }
    void keyPressEvent(QKeyEvent* event) override
    {
        if (event->key()==Qt::Key_Delete && removeSelected) { removeSelected(); event->accept(); return; }
        if (event->key()==Qt::Key_Space && currentIndex().isValid()) {
            const auto active=currentIndex().siblingAtColumn(0);
            if (active.flags().testFlag(Qt::ItemIsEnabled) && active.flags().testFlag(Qt::ItemIsUserCheckable))
                model()->setData(active,active.data(Qt::CheckStateRole).toInt()==Qt::Checked ? Qt::Unchecked : Qt::Checked,Qt::CheckStateRole);
            event->accept(); return;
        }
        QListView::keyPressEvent(event);
    }
private:
    QPersistentModelIndex m_pressed;
    int m_target=0;
};

class InstalledContentView final : public QWidget {
public:
    InstalledContentView(QSortFilterProxyModel* model,QItemSelectionModel* selection,const QStringList& columns,
                         const QList<QAction*>& actions,const QString& category,const std::function<void()>& browse,QWidget* parent=nullptr)
        : QWidget(parent)
    {
        m_model=model;
        setAcceptDrops(true);
        for (auto* action : actions) m_actions.append(action);
        setObjectName("pxlInstalledContent");
        auto* layout=new QVBoxLayout(this); layout->setContentsMargins(0,0,0,0); layout->setSpacing(12);
        auto* tools=new QHBoxLayout; tools->setSpacing(10);
        auto* search=new LibrarySearch(this); search->setObjectName("installedContentSearch"); search->setPlaceholderText(installedText("Search installed content")); tools->addWidget(search,1);
        auto* browseButton=new QPushButton(QIcon(":/pxl/icons/magnifying-glass.svg"),installedText("Browse content"),this); browseButton->setObjectName("installedBrowse"); tools->addWidget(browseButton);
        auto findAction=[&](const char* name)->QAction* { for (auto* action : actions) if (action->objectName()==name) return action; return nullptr; };
        auto* addAction=findAction("actionAddItem");
        auto* add=new QPushButton(QIcon(":/pxl/icons/plus.svg"),installedText("Add files"),this); add->setObjectName("installedAddFiles"); tools->addWidget(add);
        auto* more=new QPushButton(QStringLiteral("•••"),this); more->setAccessibleName(installedText("Content actions")); tools->addWidget(more);
        layout->addLayout(tools);
        connect(browseButton,&QPushButton::clicked,this,browse);
        if (addAction) connect(add,&QPushButton::clicked,addAction,&QAction::trigger);
        add->setEnabled(addAction && addAction->isEnabled());
        if (addAction) connect(addAction,&QAction::changed,add,[add,addAction] { add->setEnabled(addAction->isEnabled()); });
        connect(search,&QLineEdit::textChanged,this,[model](const QString& query) { model->setFilterRegularExpression(QRegularExpression(QRegularExpression::escape(query),QRegularExpression::CaseInsensitiveOption)); });
        m_stack=new QStackedWidget(this); layout->addWidget(m_stack,1);
        m_list=new InstalledContentList(m_stack); m_list->setObjectName("installedContentList");
        m_list->setModel(model); m_list->setModelColumn(qMax(0,columns.indexOf("Name"))); m_list->setSelectionModel(selection);
        m_list->setAcceptDrops(true); m_list->setDragDropMode(QAbstractItemView::DropOnly);
        m_list->setSelectionMode(QAbstractItemView::ExtendedSelection); m_list->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_list->setEditTriggers(QAbstractItemView::NoEditTriggers); m_list->setUniformItemSizes(true); m_list->setMouseTracking(true);
        m_list->setItemDelegate(new InstalledContentDelegate(columns,m_list));
        m_list->setStyleSheet("QListView { background: transparent; border: none; outline: none; } QListView::item { background: transparent; border: none; }");
        if (auto* remove=findAction("actionRemoveItem")) {
            const QPointer<QAction> command(remove);
            m_list->removeSelected=[command] { if (command) command->trigger(); };
        }
        m_stack->addWidget(m_list);
        auto* empty=new QWidget(m_stack); auto* emptyLayout=new QVBoxLayout(empty); emptyLayout->setSpacing(14); emptyLayout->addStretch();
        auto* icon=new QLabel(empty); icon->setAlignment(Qt::AlignCenter); icon->setPixmap(QIcon(":/pxl/icons/squares-four.svg").pixmap(48,48)); emptyLayout->addWidget(icon);
        auto* title=new QLabel(empty); title->setObjectName("installedEmptyTitle"); title->setAlignment(Qt::AlignCenter); title->setStyleSheet("font-size: 20px; font-weight: 600;"); emptyLayout->addWidget(title);
        auto* hint=new QLabel(installedText("Find additions for this instance or add files from your computer."),empty); hint->setWordWrap(true); hint->setAlignment(Qt::AlignCenter); hint->setProperty("pxlFlowHint",true); emptyLayout->addWidget(hint);
        auto* emptyActions=new QHBoxLayout; emptyActions->addStretch();
        auto* find=new QPushButton(installedText("Browse content"),empty); find->setProperty("pxlPrimary",true); emptyActions->addWidget(find);
        emptyActions->addStretch(); emptyLayout->addLayout(emptyActions); emptyLayout->addStretch(); m_stack->addWidget(empty);
        connect(find,&QPushButton::clicked,this,browse);
        const auto update=[=,this] {
            const bool installed=model->sourceModel()->rowCount()>0;
            const bool visible=model->rowCount()>0;
            m_stack->setCurrentIndex(visible ? 0 : 1);
            const auto label=category=="mods" ? installedText("No mods installed") : category=="shaderpacks" ? installedText("No shaders installed") : installedText("No resource packs installed");
            title->setText(installed ? installedText("No matching content") : label);
            hint->setVisible(!installed); find->setVisible(!installed);
        };
        connect(model,&QAbstractItemModel::rowsInserted,this,update); connect(model,&QAbstractItemModel::rowsRemoved,this,update);
        connect(model,&QAbstractItemModel::modelReset,this,update); connect(model,&QAbstractItemModel::layoutChanged,this,update);
        for (auto signal : {&QAbstractItemModel::rowsInserted,&QAbstractItemModel::rowsRemoved})
            connect(model->sourceModel(),signal,this,[this,update] { QTimer::singleShot(0,this,update); });
        connect(model->sourceModel(),&QAbstractItemModel::modelReset,this,[this,update] { QTimer::singleShot(0,this,update); });
        update();
        m_list->showActions=[this](const QModelIndex& index,const QPoint& at) {
            if (!m_list->selectionModel()->isRowSelected(index.row(),index.parent()))
                m_list->selectionModel()->select(index,QItemSelectionModel::ClearAndSelect|QItemSelectionModel::Rows);
            m_list->selectionModel()->setCurrentIndex(index,QItemSelectionModel::NoUpdate); showMenu(at);
        };
        m_list->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_list,&QWidget::customContextMenuRequested,this,[this](const QPoint& point) {
            const auto index=m_list->indexAt(point); if (index.isValid()) m_list->showActions(index,m_list->viewport()->mapToGlobal(point));
        });
        connect(more,&QPushButton::clicked,this,[this,more] { showMenu(more->mapToGlobal(QPoint(0,more->height()+4))); });
    }
protected:
    void dragEnterEvent(QDragEnterEvent* event) override
    {
        if (event->mimeData()->hasUrls()) event->acceptProposedAction();
    }
    void dropEvent(QDropEvent* event) override
    {
        if (m_model && m_model->dropMimeData(event->mimeData(),event->proposedAction(),-1,-1,{})) event->acceptProposedAction();
    }
private:
    void showMenu(const QPoint& at)
    {
        FrostedMenu menu(this); const auto flags=menu.windowFlags(); menu.setParent(nullptr,flags);
        connect(this,&QObject::destroyed,&menu,&QMenu::close);
        for (const auto& source : m_actions) {
            if (!source || !source->isVisible() || source->objectName()=="actionDownloadItem") continue;
            if (source->isSeparator()) { menu.addSeparator(); continue; }
            if (source->text().isEmpty()) continue;
            forwardMenuAction(&menu,source,QString(source->text()).remove('&'));
        }
        menu.exec(at);
    }
    QPointer<QSortFilterProxyModel> m_model;
    QList<QPointer<QAction>> m_actions;
    InstalledContentList* m_list;
    QStackedWidget* m_stack;
};
} // namespace Pxl

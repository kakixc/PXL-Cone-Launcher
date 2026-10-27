// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "LibraryControls.h"
#include "ActionMenu.h"
#include <QSortFilterProxyModel>
#include <QItemSelectionModel>
#include <QListView>
#include <QStyledItemDelegate>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QDateTime>
#include <QLocale>
#include <functional>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QHelpEvent>
#include <QToolTip>
#include <QTimer>

namespace Pxl {
inline QString worldsText(const char* text) { return QCoreApplication::translate("Pxl::WorldsView", text); }
inline QRect worldMore(const QRect& row) { return {row.right() - 48, row.center().y() - 16, 32, 32}; }
class WorldDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex& index) const override
    {
        return {360, index.siblingAtColumn(4).data().toString().isEmpty() ? 86 : 108};
    }
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        painter->save(); painter->setRenderHint(QPainter::Antialiasing);
        const auto area = option.rect.adjusted(1, 3, -1, -3);
        painter->setBrush(QColor(31, 35, 41, option.state.testFlag(QStyle::State_MouseOver) ? 220 : 185));
        painter->setPen(option.state.testFlag(QStyle::State_Selected) ? QColor(242, 163, 101, 110) : QColor(199, 210, 225, 24));
        painter->drawRoundedRect(area, 13, 13);
        auto icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        if (icon.isNull()) icon = QIcon(":/pxl/icons/squares-four.svg");
        icon.paint(painter, QRect(area.left() + 16, area.top() + 15, 48, 48));
        const int left = area.left() + 82;
        const int width = qMax(10, worldMore(option.rect).left() - left - 16);
        auto font = option.font; font.setPixelSize(15); font.setWeight(QFont::DemiBold);
        painter->setFont(font); painter->setPen(QColor("#edf0f4"));
        painter->drawText(QRect(left, area.top() + 12, width, 24), Qt::AlignVCenter,
                          QFontMetrics(font).elidedText(index.data().toString(), Qt::ElideRight, width));
        font.setPixelSize(12); font.setWeight(QFont::Normal); painter->setFont(font); painter->setPen(QColor("#aab5c4"));
        const auto played = index.siblingAtColumn(2).data().toDateTime();
        const auto detail = QStringList{index.siblingAtColumn(1).data().toString(), index.siblingAtColumn(3).data().toString(),
            played.isValid() ? QLocale().toString(played, QLocale::ShortFormat) : worldsText("Not played yet")}.join(" · ");
        painter->drawText(QRect(left, area.top() + 39, width, 20), Qt::AlignVCenter,
                          QFontMetrics(font).elidedText(detail, Qt::ElideRight, width));
        const auto warning = index.siblingAtColumn(4).data().toString().trimmed();
        if (!warning.isEmpty()) {
            painter->setPen(QColor("#e6b288"));
            painter->drawText(QRect(left, area.top() + 65, width, 20), Qt::AlignVCenter,
                              QFontMetrics(font).elidedText(warning, Qt::ElideRight, width));
        }
        QIcon(":/pxl/icons/dots-three-vertical.svg").paint(painter, worldMore(option.rect).adjusted(6, 6, -6, -6));
        painter->restore();
    }
};
class WorldRows final : public QListView {
public:
    using QListView::QListView;
    std::function<void(const QModelIndex&, const QPoint&)> showActions;
    std::function<void()> removeCurrent;
protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        m_menuPressed = QPersistentModelIndex();
        const auto index = indexAt(event->position().toPoint());
        if (event->button() == Qt::LeftButton && index.isValid() && worldMore(visualRect(index)).contains(event->position().toPoint())) {
            m_menuPressed = index; event->accept(); return;
        }
        QListView::mousePressEvent(event);
    }
    bool viewportEvent(QEvent* event) override
    {
        if (event->type() == QEvent::ToolTip) {
            auto* help = static_cast<QHelpEvent*>(event);
            const auto index = indexAt(help->pos());
            if (index.isValid() && !index.siblingAtColumn(4).data().toString().isEmpty()) {
                QToolTip::showText(help->globalPos(), "<qt>" + index.siblingAtColumn(4).data(Qt::ToolTipRole).toString().toHtmlEscaped().replace('\n', "<br>") + "</qt>", this);
                return true;
            }
        }
        return QListView::viewportEvent(event);
    }
    void mouseReleaseEvent(QMouseEvent* event) override
    {
        const auto index = indexAt(event->position().toPoint());
        const auto pressed = m_menuPressed; m_menuPressed = QPersistentModelIndex();
        if (event->button() == Qt::LeftButton && index.isValid() && index == pressed && worldMore(visualRect(index)).contains(event->position().toPoint())) {
            if (showActions) showActions(index, viewport()->mapToGlobal(event->position().toPoint()));
            event->accept(); return;
        }
        QListView::mouseReleaseEvent(event);
    }
    void keyPressEvent(QKeyEvent* event) override
    {
        if (event->key() == Qt::Key_Delete && removeCurrent) { removeCurrent(); event->accept(); return; }
        QListView::keyPressEvent(event);
    }
private:
    QPersistentModelIndex m_menuPressed;
};
class WorldsView final : public QWidget {
public:
    WorldsView(QSortFilterProxyModel* model, QItemSelectionModel* selection, const QList<QAction*>& actions, QWidget* parent)
        : QWidget(parent), m_model(model)
    {
        setObjectName("pxlWorldsView");
        setAcceptDrops(true);
        for (auto* command : actions) m_actions.append(command);
        auto* layout = new QVBoxLayout(this); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(14);
        auto* tools = new QHBoxLayout;
        auto* search = new LibrarySearch(this); search->setObjectName("worldSearch"); search->setPlaceholderText(worldsText("Search worlds")); tools->addWidget(search, 1);
        auto* sorting = new ChoiceChip(this); sorting->setObjectName("worldSorting");
        for (int column = 0; column < model->columnCount(); ++column) {
            const auto title = model->headerData(column, Qt::Horizontal).toString();
            sorting->addItem(title + " ↑"); sorting->addItem(title + " ↓");
        }
        sorting->setCurrentIndex(qMax(0, model->sortColumn()) * 2 + (model->sortOrder() == Qt::DescendingOrder ? 1 : 0));
        connect(sorting, qOverload<int>(&QComboBox::activated), model, [model](int index) { model->sort(index / 2, index % 2 ? Qt::DescendingOrder : Qt::AscendingOrder); });
        tools->addWidget(button("actionAdd", worldsText("Import world")));
        tools->addWidget(button("actionRefresh", worldsText("Refresh")));
        tools->addWidget(button("actionView_Folder", worldsText("Open folder")));
        layout->addLayout(tools);
        auto* secondary = new QHBoxLayout; secondary->addWidget(sorting); secondary->addStretch();
        for (const auto& command : m_actions) if (command && command->objectName() == "actionJoin")
            secondary->addWidget(button("actionJoin", worldsText("Play world")));
        layout->addLayout(secondary);
        m_stack = new QStackedWidget(this); layout->addWidget(m_stack, 1);
        m_list = new WorldRows(m_stack); m_list->setObjectName("worldRows"); m_list->setModel(model); m_list->setSelectionModel(selection);
        m_list->setModelColumn(0); m_list->setSelectionMode(QAbstractItemView::SingleSelection); m_list->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_list->setEditTriggers(QAbstractItemView::NoEditTriggers); m_list->setMouseTracking(true);
        m_list->setDragDropMode(QAbstractItemView::DragDrop); m_list->setDragEnabled(true); m_list->setAcceptDrops(true);
        m_list->setItemDelegate(new WorldDelegate(m_list));
        m_list->setStyleSheet("QListView { background: transparent; border: none; outline: none; } QListView::item { background: transparent; }");
        m_stack->addWidget(m_list);
        auto* empty = new QWidget(m_stack); auto* emptyLayout = new QVBoxLayout(empty); emptyLayout->setSpacing(14); emptyLayout->addStretch();
        auto* icon = new QLabel(empty); icon->setPixmap(QIcon(":/pxl/icons/squares-four.svg").pixmap(48, 48)); icon->setAlignment(Qt::AlignCenter); emptyLayout->addWidget(icon);
        auto* title = new QLabel(empty); title->setObjectName("worldEmptyTitle"); title->setAlignment(Qt::AlignCenter); title->setStyleSheet("font-size: 20px; font-weight: 600;"); emptyLayout->addWidget(title);
        auto* hint = new QLabel(worldsText("Create a world in Minecraft or import a world archive."), empty); hint->setAlignment(Qt::AlignCenter); hint->setWordWrap(true); hint->setProperty("pxlFlowHint", true); emptyLayout->addWidget(hint);
        auto* importWorld = button("actionAdd", worldsText("Import world")); importWorld->setParent(empty); emptyLayout->addWidget(importWorld, 0, Qt::AlignHCenter);
        emptyLayout->addStretch(); m_stack->addWidget(empty);
        const auto refresh = [=, this] {
            const bool any = model->sourceModel()->rowCount() > 0;
            m_stack->setCurrentIndex(model->rowCount() ? 0 : 1);
            title->setText(any ? worldsText("No matching worlds") : worldsText("No worlds yet"));
            hint->setVisible(!any); importWorld->setVisible(!any);
        };
        connect(model, &QAbstractItemModel::rowsInserted, this, refresh); connect(model, &QAbstractItemModel::rowsRemoved, this, refresh);
        connect(model, &QAbstractItemModel::modelReset, this, refresh); connect(model, &QAbstractItemModel::layoutChanged, this, refresh);
        for (auto signal : {&QAbstractItemModel::rowsInserted, &QAbstractItemModel::rowsRemoved})
            connect(model->sourceModel(), signal, this, [this, refresh] { QTimer::singleShot(0, this, refresh); });
        connect(model->sourceModel(), &QAbstractItemModel::modelReset, this, [this, refresh] { QTimer::singleShot(0, this, refresh); });
        connect(search, &QLineEdit::textChanged, model, &QSortFilterProxyModel::setFilterFixedString);
        m_list->showActions = [this](const QModelIndex& index, const QPoint& point) {
            m_list->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            showMenu(point);
        };
        m_list->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_list, &QWidget::customContextMenuRequested, this, [this](const QPoint& point) {
            const auto index = m_list->indexAt(point); if (index.isValid()) m_list->showActions(index, m_list->viewport()->mapToGlobal(point));
        });
        for (const auto& command : m_actions) {
            if (!command) continue;
            if (command->objectName() == "actionRemove") m_list->removeCurrent = [command] { if (command && command->isEnabled()) command->trigger(); };
            if (command->objectName() == "actionJoin") connect(m_list, &QAbstractItemView::activated, this, [command](const QModelIndex&) { if (command && command->isEnabled()) command->trigger(); });
        }
        refresh();
    }
protected:
    void dragEnterEvent(QDragEnterEvent* event) override { if (event->mimeData()->hasUrls()) event->acceptProposedAction(); }
    void dropEvent(QDropEvent* event) override
    {
        if (m_model && m_model->dropMimeData(event->mimeData(), event->proposedAction(), -1, -1, {})) event->acceptProposedAction();
    }
private:
    QPushButton* button(const char* name, const QString& label)
    {
        auto* result = new QPushButton(label, this); result->setObjectName(name); result->setEnabled(false);
        for (const auto& command : m_actions) if (command && command->objectName() == name) {
            result->setEnabled(command->isEnabled());
            connect(command, &QAction::changed, result, [result, command] { result->setEnabled(command && command->isEnabled()); });
            connect(result, &QPushButton::clicked, command, &QAction::trigger); break;
        }
        return result;
    }
    void showMenu(const QPoint& point)
    {
        FrostedMenu menu(this); const auto flags = menu.windowFlags(); menu.setParent(nullptr, flags);
        connect(this, &QObject::destroyed, &menu, &QMenu::close);
        for (const auto& command : m_actions) if (command && command->isVisible()) {
            if (command->isSeparator()) menu.addSeparator();
            else if (!command->text().isEmpty()) forwardMenuAction(&menu, command, QString(command->text()).remove('&'));
        }
        menu.exec(point);
    }
    QList<QPointer<QAction>> m_actions;
    QPointer<QSortFilterProxyModel> m_model;
    WorldRows* m_list;
    QStackedWidget* m_stack;
};
} // namespace Pxl

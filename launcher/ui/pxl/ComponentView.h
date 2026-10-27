// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "InstalledContentView.h"
#include <QAbstractProxyModel>
#include <QPlainTextEdit>

namespace Pxl {
inline QString componentText(const char* text) { return QCoreApplication::translate("Pxl::ComponentView", text); }
inline int componentSourceRow(QModelIndex index)
{
    while (index.isValid()) {
        const auto* proxy = qobject_cast<const QAbstractProxyModel*>(index.model());
        if (!proxy) return index.row();
        index = proxy->mapToSource(index);
    }
    return -1;
}

class ComponentDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override { return {360, 76}; }
    bool editorEvent(QEvent*, QAbstractItemModel*, const QStyleOptionViewItem&, const QModelIndex&) override { return false; }
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const auto area = option.rect.adjusted(1, 3, -1, -3);
        painter->setBrush(QColor(30, 35, 41, option.state.testFlag(QStyle::State_MouseOver) ? 225 : 180));
        painter->setPen(option.state.testFlag(QStyle::State_Selected) ? QColor(242, 163, 101, 110) : QColor(200, 210, 225, 25));
        painter->drawRoundedRect(area, 12, 12);
        auto icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        if (icon.isNull()) icon = QIcon(":/pxl/icons/squares-four.svg");
        icon.paint(painter, QRect(area.left() + 16, area.center().y() - 16, 32, 32));
        const int left = area.left() + 64;
        const int width = qMax(10, installedToggle(option.rect).left() - left - 20);
        auto font = option.font;
        font.setPixelSize(15);
        font.setWeight(QFont::DemiBold);
        painter->setFont(font);
        painter->setPen(QColor("#edf0f4"));
        painter->drawText(QRect(left, area.top() + 11, width, 24), Qt::AlignVCenter,
                          QFontMetrics(font).elidedText(index.data().toString(), Qt::ElideRight, width));
        font.setPixelSize(12);
        font.setWeight(QFont::Normal);
        painter->setFont(font);
        painter->setPen(QColor("#aab5c4"));
        const auto version = componentText("Version %1").arg(index.siblingAtColumn(1).data().toString());
        painter->drawText(QRect(left, area.top() + 37, width, 20), Qt::AlignVCenter,
                          QFontMetrics(font).elidedText(version, Qt::ElideRight, width));
        const bool optional = index.flags().testFlag(Qt::ItemIsUserCheckable);
        const bool checked = index.data(Qt::CheckStateRole).toInt() == Qt::Checked;
        if (optional) {
            const auto toggle = installedToggle(option.rect);
            painter->setPen(Qt::NoPen);
            painter->setBrush(checked ? QColor("#d29463") : QColor("#535d69"));
            painter->drawRoundedRect(toggle, 11, 11);
            painter->setBrush(checked ? QColor("#251c15") : QColor("#d5dce5"));
            painter->drawEllipse(QRect(checked ? toggle.right() - 18 : toggle.left() + 3, toggle.top() + 3, 16, 16));
        } else if (checked) {
            const auto center = installedToggle(option.rect).center();
            painter->setPen(QPen(QColor("#d8b18e"), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter->drawLine(center + QPoint(-6, 0), center + QPoint(-2, 4));
            painter->drawLine(center + QPoint(-2, 4), center + QPoint(6, -5));
        } else {
            painter->setPen(QPen(QColor("#aab5c4"), 2));
            const auto center = installedToggle(option.rect).center();
            painter->drawLine(center - QPoint(6, 0), center + QPoint(6, 0));
        }
        QIcon(":/pxl/icons/dots-three-vertical.svg").paint(painter, installedMore(option.rect).adjusted(6, 6, -6, -6));
        painter->restore();
    }
};

class ComponentView final : public QWidget {
public:
    ComponentView(QSortFilterProxyModel* model, QItemSelectionModel* selection, const QList<QAction*>& actions, QWidget* parent)
        : QWidget(parent)
    {
        setObjectName("pxlComponentView");
        for (auto* action : actions) m_actions.append(action);
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(14);
        auto* tools = new QHBoxLayout;
        auto* search = new LibrarySearch(this);
        search->setObjectName("componentSearch");
        search->setPlaceholderText(componentText("Search components"));
        tools->addWidget(search, 1);
        auto* loader = commandButton("actionInstall_Loader", componentText("Install loader"));
        tools->addWidget(loader);
        tools->addWidget(commandButton("actionChange_version", componentText("Change version")));
        auto* more = new QPushButton(componentText("More actions"), this);
        tools->addWidget(more);
        layout->addLayout(tools);
        m_list = new InstalledContentList(this);
        m_list->setObjectName("componentList");
        m_list->setModel(model);
        m_list->setModelColumn(0);
        m_list->setSelectionModel(selection);
        m_list->setSelectionMode(QAbstractItemView::SingleSelection);
        m_list->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_list->setMouseTracking(true);
        m_list->setUniformItemSizes(true);
        m_list->setItemDelegate(new ComponentDelegate(m_list));
        m_list->setStyleSheet("QListView { background: transparent; border: none; outline: none; } QListView::item { background: transparent; }");
        layout->addWidget(m_list, 1);
        m_problem = new QPlainTextEdit(this);
        m_problem->setReadOnly(true);
        m_problem->setMaximumHeight(120);
        m_problem->setStyleSheet("color: #f0bb8e; padding: 12px; background: rgba(60,43,31,150); border-radius: 10px;");
        m_problem->hide();
        layout->addWidget(m_problem);
        auto* hint = new QLabel(componentText("Core components stay enabled. Select a row to change its version or open its actions."), this);
        hint->setWordWrap(true);
        hint->setProperty("pxlFlowHint", true);
        layout->addWidget(hint);
        connect(search, &QLineEdit::textChanged, model, &QSortFilterProxyModel::setFilterFixedString);
        m_list->showActions = [this](const QModelIndex& index, const QPoint& at) {
            m_list->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            showMenu(at);
        };
        m_list->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_list, &QWidget::customContextMenuRequested, this, [this](const QPoint& point) {
            const auto index = m_list->indexAt(point);
            if (index.isValid()) m_list->showActions(index, m_list->viewport()->mapToGlobal(point));
        });
        connect(more, &QPushButton::clicked, this, [this, more] { showMenu(more->mapToGlobal(QPoint(0, more->height() + 4))); });
    }
    void setProblem(const QString& text) { m_problem->setPlainText(text); m_problem->setVisible(!text.isEmpty()); }
private:
    QPushButton* commandButton(const char* name, const QString& label)
    {
        auto* button = new QPushButton(label, this);
        button->setObjectName(name);
        button->setEnabled(false);
        for (const auto& command : m_actions) {
            if (!command || command->objectName() != name) continue;
            button->setEnabled(command->isEnabled());
            connect(command, &QAction::changed, button, [button, command] { button->setEnabled(command && command->isEnabled()); });
            connect(button, &QPushButton::clicked, command, &QAction::trigger);
            break;
        }
        return button;
    }
    void showMenu(const QPoint& point)
    {
        FrostedMenu menu(this);
        const auto flags = menu.windowFlags();
        menu.setParent(nullptr, flags);
        connect(this, &QObject::destroyed, &menu, &QMenu::close);
        for (const auto& command : m_actions) {
            if (!command || !command->isVisible()) continue;
            if (command->isSeparator()) menu.addSeparator();
            else if (!command->text().isEmpty()) forwardMenuAction(&menu, command, QString(command->text()).remove('&'));
        }
        menu.exec(point);
    }
    QList<QPointer<QAction>> m_actions;
    InstalledContentList* m_list;
    QPlainTextEdit* m_problem;
};
} // namespace Pxl

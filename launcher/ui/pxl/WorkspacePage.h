// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QPoint>
#include <QString>
#include <QWidget>

class InstanceList;
class QModelIndex;
class QComboBox;
class QLineEdit;
class QLabel;
class QListView;
class QPushButton;
class QSortFilterProxyModel;
class QStackedWidget;
class WorkspaceCatPreview;
class QScrollArea;
class QResizeEvent;
class QEvent;

/**
 * A compact Home and Library surface over the existing instance list.
 *
 * MainWindow remains responsible for creating, importing, exporting and
 * launching instances. This widget only presents model data and emits intent.
 */
class WorkspacePage final : public QWidget {
    Q_OBJECT

   public:
    explicit WorkspacePage(InstanceList* instances, QWidget* parent = nullptr);

    void setSelectedInstance(const QString& id);
    void refresh();
    void setPaintCat(bool visible);
    void setCatButtonVisible(bool visible);

   signals:
    void selectInstance(QString id);
    void activateInstance(QString id);
    void contextMenuRequested(QString id, QPoint global);
    void createRequested();
    void importRequested();
    void exportRequested();
    void accountsRequested();
    void settingsRequested();
    void classicRequested();
    void catRequested();

   protected:
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* object, QEvent* event) override;

   private:
    QString idForIndex(const QModelIndex& index) const;
    void selectInView(QListView* view, const QString& id);
    void syncSelection(QListView* source, const QModelIndex& current);
    void updateGroups();
    void updateHome();
    void showContextMenu(QListView* view, const QPoint& position);
    void updateLayout();
    void showHome(bool home);

    InstanceList* m_instances = nullptr;
    QSortFilterProxyModel* m_recentModel = nullptr;
    QSortFilterProxyModel* m_libraryModel = nullptr;
    QListView* m_homeView = nullptr;
    QListView* m_libraryView = nullptr;
    QLabel* m_homeEmpty = nullptr;
    QLabel* m_libraryEmpty = nullptr;
    QLineEdit* m_search = nullptr;
    QComboBox* m_groups = nullptr;
    QPushButton* m_playSelected = nullptr;
    QPushButton* m_moreSelected = nullptr;
    QWidget* m_recentSection = nullptr;
    QWidget* m_emptyState = nullptr;
    QWidget* m_selectionBar = nullptr;
    QLabel* m_heading = nullptr;
    QLabel* m_count = nullptr;
    QLabel* m_selectedName = nullptr;
    QLabel* m_selectedDetail = nullptr;
    QLabel* m_selectedIcon = nullptr;
    QScrollArea* m_scroll = nullptr;
    QPushButton* m_homeButton = nullptr;
    QPushButton* m_libraryButton = nullptr;
    QPushButton* m_catButton = nullptr;
    bool m_showHome = true;
    WorkspaceCatPreview* m_catPreview = nullptr;
    QString m_selectedId;
    bool m_syncingSelection = false;
};

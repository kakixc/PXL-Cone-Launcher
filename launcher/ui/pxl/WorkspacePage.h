// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QPoint>
#include <QString>
#include <QWidget>
#include <QPixmap>
#include <QPointer>
#include <QSet>
#include <functional>

class InstanceList;
class BaseInstance;
class SettingsObject;
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
class QBoxLayout;

/**
 * A compact Home and Library surface over the existing instance list.
 *
 * MainWindow remains responsible for creating, importing, exporting and
 * launching instances. This widget only presents model data and emits intent.
 */
class WorkspacePage final : public QWidget {
    Q_OBJECT

   public:
    explicit WorkspacePage(InstanceList* instances, QWidget* parent = nullptr, SettingsObject* preferences = nullptr);
    ~WorkspacePage() override;

    void setSelectedInstance(const QString& id);
    void refresh();
    void setPaintCat(bool visible);
    void setCatButtonVisible(bool visible);
    void setImageBackground(bool enabled, const QString& path = {});
    void setNavigationPosition(bool top);
    void setLibraryViewMode(bool cards);
    void setLibraryPresentation(const QString& mode);
    bool presentInstanceView(QWidget* view, const QString& title, std::function<bool()> leaveGuard);
    bool leaveInstanceView();
    void updateActiveNavigation();

   signals:
    void selectInstance(QString id);
    void activateInstance(QString id);
    void stopInstance(QString id);
    void contextMenuRequested(QString id, QPoint global);
    void createRequested();
    void importRequested();
    void exportRequested();
    void accountsRequested();
    void settingsRequested();
    void classicRequested();
    void catRequested();
    void openInstanceRequested(QString id);
    void browseModsRequested(QString id);

   protected:
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* object, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void changeEvent(QEvent* event) override;

   private:
    QString idForIndex(const QModelIndex& index) const;
    void selectInView(QListView* view, const QString& id);
    void syncSelection(QListView* source, const QModelIndex& current);
    void updateGroups();
    void updateHome();
    void showContextMenu(QListView* view, const QPoint& position);
    void updateLayout();
    void showHome(bool home);
    void retranslateUi();

    InstanceList* m_instances = nullptr;
    QSortFilterProxyModel* m_recentModel = nullptr;
    QSortFilterProxyModel* m_libraryModel = nullptr;
    QListView* m_homeView = nullptr;
    QListView* m_libraryView = nullptr;
    QLabel* m_libraryEmpty = nullptr;
    QLineEdit* m_search = nullptr;
    QComboBox* m_groups = nullptr;
    QPushButton* m_featuredPlay = nullptr;
    QWidget* m_recentSection = nullptr;
    QWidget* m_emptyState = nullptr;
    QWidget* m_libraryFilters = nullptr;
    QWidget* m_navigation = nullptr;
    QWidget* m_header = nullptr;
    QBoxLayout* m_rootLayout = nullptr;
    QBoxLayout* m_navigationLayout = nullptr;
    QList<QPushButton*> m_navigationButtons;
    QLabel* m_heading = nullptr;
    QLabel* m_count = nullptr;
    QLabel* m_featuredName = nullptr;
    QLabel* m_featuredDetail = nullptr;
    QLabel* m_featuredTime = nullptr;
    QLabel* m_libraryTitle = nullptr;
    QScrollArea* m_scroll = nullptr;
    QStackedWidget* m_contentStack = nullptr;
    QPointer<QWidget> m_instanceView;
    QString m_instanceViewTitle;
    std::function<bool()> m_leaveInstanceGuard;
    QPushButton* m_backButton = nullptr;
    QPushButton* m_homeButton = nullptr;
    QPushButton* m_libraryButton = nullptr;
    QPushButton* m_catButton = nullptr;
    bool m_showHome = true;
    bool m_imageBackground = false;
    QPixmap m_backgroundImage;
    WorkspaceCatPreview* m_catPreview = nullptr;
    QString m_selectedId;
    QString m_featuredId;
    bool m_syncingSelection = false;
    bool m_cardsMode = false;
    QComboBox* m_viewModeButton = nullptr;
    QLabel* m_totalTime = nullptr;
    QSet<BaseInstance*> m_observedInstances;
    QPointer<SettingsObject> m_preferences;
};

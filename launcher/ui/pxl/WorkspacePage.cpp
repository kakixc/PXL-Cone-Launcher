// SPDX-License-Identifier: GPL-3.0-only

#include "WorkspacePage.h"

#include <algorithm>

#include <Application.h>
#include <BaseInstance.h>
#include <InstanceList.h>
#include <icons/IconList.h>
#include <minecraft/MinecraftInstance.h>
#include <minecraft/PackProfile.h>
#include <modplatform/ModIndex.h>
#include <ui/themes/CatPainter.h>
#include <ui/themes/ThemeManager.h>

#include <QApplication>
#include <QDateTime>
#include <QMouseEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QResizeEvent>
#include <QTimer>
#include <QFontMetrics>
#include <functional>
#include <QComboBox>
#include <QFrame>
#include <QHash>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPainter>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QVBoxLayout>

static void initializeWorkspaceResources()
{
    static const bool initialized = [] {
        Q_INIT_RESOURCE(fonts);
        Q_INIT_RESOURCE(breeze_dark);
        return true;
    }();
    Q_UNUSED(initialized);
}

class WorkspaceCatPreview final : public QWidget {
   public:
    explicit WorkspaceCatPreview(QWidget* parent) : QWidget(parent) { setMinimumHeight(88); }

    void setPaintCat(bool visible)
    {
        delete m_cat;
        m_cat = nullptr;
        if (visible) {
            if (auto* app = APPLICATION_DYN; app && app->themeManager()) {
                m_cat = new CatPainter(app->themeManager()->getCatPack(), this);
                connect(m_cat, &CatPainter::updateFrame, this, qOverload<>(&QWidget::update));
            }
        }
        update();
    }

   protected:
    void paintEvent(QPaintEvent* event) override
    {
        QWidget::paintEvent(event);
        if (!m_cat)
            return;
        QPainter painter(this);
        m_cat->paint(&painter, rect());
    }

   private:
    CatPainter* m_cat = nullptr;
};

namespace {
constexpr auto kAllGroups = "__pxl_all_groups__";
constexpr auto kWorkspaceMetadataRole = Qt::UserRole + 0x5a;

class WorkspaceInstanceModel final : public QSortFilterProxyModel {
   public:
    enum class Mode { Recent, Library };

    explicit WorkspaceInstanceModel(Mode mode, QObject* parent) : QSortFilterProxyModel(parent), m_mode(mode)
    {
        setDynamicSortFilter(true);
        setSortCaseSensitivity(Qt::CaseInsensitive);
    }

    void setSearchText(const QString& value)
    {
        if (m_search == value)
            return;
        m_search = value;
        invalidateFilter();
    }

    void setGroup(const QString& value)
    {
        if (m_group == value)
            return;
        m_group = value;
        invalidateFilter();
    }

    void rebuildMetadata()
    {
        m_metadata.clear();
        if (!sourceModel())
            return;
        for (int row = 0; row < sourceModel()->rowCount(); ++row) {
            const auto index = sourceModel()->index(row, 0);
            const auto id = index.data(InstanceList::InstanceIDRole).toString();
            auto* instance = static_cast<BaseInstance*>(index.data(InstanceList::InstancePointerRole).value<void*>());
            if (id.isEmpty() || !instance)
                continue;
            auto detail = instance->typeName();
            if (auto* minecraft = dynamic_cast<MinecraftInstance*>(instance)) {
                // PackProfile is already maintained by MinecraftInstance. Reading it here,
                // during a model refresh, avoids any profile work from the paint path.
                if (auto* profile = minecraft->getPackProfile()) {
                    const auto minecraftVersion = profile->getComponentVersion("net.minecraft");
                    const auto loaders = profile->getModLoadersList();
                    QStringList loaderNames;
                    for (const auto loader : loaders)
                        loaderNames.append(ModPlatform::getModLoaderAsString(loader));
                    detail = minecraftVersion.isEmpty() ? tr("Minecraft") : tr("Minecraft %1").arg(minecraftVersion);
                    if (!loaderNames.isEmpty())
                        detail.append(" · " + loaderNames.join(", "));
                }
            }
            m_metadata.insert(id, detail);
        }
    }

   protected:
    QVariant data(const QModelIndex& index, int role) const override
    {
        if (role == Qt::DecorationRole) {
            const auto key = QSortFilterProxyModel::data(index, role).toString();
            if (auto* app = APPLICATION_DYN; app && app->icons())
                return app->icons()->getIcon(key);
            return QIcon(":/icons/breeze_dark/scalable/minecraft.svg");
        }
        if (role == kWorkspaceMetadataRole)
            return m_metadata.value(QSortFilterProxyModel::data(index, InstanceList::InstanceIDRole).toString());
        if (role == Qt::AccessibleTextRole) {
            const auto name = QSortFilterProxyModel::data(index, Qt::DisplayRole).toString();
            const auto group = QSortFilterProxyModel::data(index, InstanceList::GroupRole).toString();
            auto* instance = static_cast<BaseInstance*>(
                QSortFilterProxyModel::data(index, InstanceList::InstancePointerRole).value<void*>());
            const auto state = instance && instance->isRunning() ? tr("running") : tr("ready");
            return group.isEmpty() ? tr("%1, %2").arg(name, state) : tr("%1, %2, %3").arg(name, group, state);
        }
        return QSortFilterProxyModel::data(index, role);
    }

    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override
    {
        const auto index = sourceModel()->index(sourceRow, 0, sourceParent);
        auto* instance = static_cast<BaseInstance*>(index.data(InstanceList::InstancePointerRole).value<void*>());
        if (!instance || instance->currentStatus() != BaseInstance::Status::Present)
            return false;
        if (m_mode == Mode::Recent && instance->lastLaunch() <= 0)
            return false;
        if (m_group != QLatin1String(kAllGroups)
            && index.data(InstanceList::GroupRole).toString() != m_group)
            return false;
        return m_search.isEmpty() || index.data(Qt::DisplayRole).toString().contains(m_search, Qt::CaseInsensitive);
    }

    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override
    {
        auto* lhs = static_cast<BaseInstance*>(left.data(InstanceList::InstancePointerRole).value<void*>());
        auto* rhs = static_cast<BaseInstance*>(right.data(InstanceList::InstancePointerRole).value<void*>());
        if (m_mode == Mode::Recent && lhs && rhs && lhs->lastLaunch() != rhs->lastLaunch())
            return lhs->lastLaunch() > rhs->lastLaunch();
        return QString::localeAwareCompare(left.data(Qt::DisplayRole).toString(), right.data(Qt::DisplayRole).toString()) < 0;
    }

   private:
    Mode m_mode;
    QString m_search;
    QString m_group = QLatin1String(kAllGroups);
    QHash<QString, QString> m_metadata;
};

QString lastPlayedText(BaseInstance* instance)
{
    if (!instance || instance->lastLaunch() <= 0)
        return QObject::tr("Not played yet");
    const auto seconds = qMax<qint64>(0, QDateTime::currentSecsSinceEpoch() - instance->lastLaunch());
    if (seconds < 60) return QObject::tr("Played just now");
    if (seconds < 3600) return QObject::tr("Played %1 min ago").arg(seconds / 60);
    if (seconds < 86400) return QObject::tr("Played %1 h ago").arg(seconds / 3600);
    return QObject::tr("Played %1 d ago").arg(seconds / 86400);
}

class WorkspaceCardDelegate final : public QStyledItemDelegate {
   public:
    using Activate = std::function<void(const QModelIndex&)>;
    using Context = std::function<void(const QModelIndex&, const QPoint&)>;
    WorkspaceCardDelegate(bool recent, Activate activate, Context context, QObject* parent)
        : QStyledItemDelegate(parent), m_recent(recent), m_activate(std::move(activate)), m_context(std::move(context)) {}

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override { return m_recent ? QSize(480, 84) : QSize(190, 250); }
    QRect playRect(const QRect& card) const
    {
        return m_recent ? QRect(card.right() - 126, card.top() + 20, 82, 36)
                        : QRect(card.left() + 22, card.top() + 116, card.width() - 44, 34);
    }
    QRect moreRect(const QRect& card) const { return m_recent ? QRect(card.right() - 38, card.top() + 20, 28, 36)
                                                                           : QRect(card.right() - 32, card.bottom() - 34, 26, 26); }
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const auto& p = option.palette;
        const QRect card = option.rect.adjusted(3, 3, -5, -5);
        const bool selected = option.state.testFlag(QStyle::State_Selected);
        const bool hover = option.state.testFlag(QStyle::State_MouseOver);
        auto* instance = static_cast<BaseInstance*>(index.data(InstanceList::InstancePointerRole).value<void*>());
        const bool launchable = instance && instance->canLaunch() && !instance->isRunning();
        const auto surface = hover ? p.color(QPalette::Button) : p.color(QPalette::AlternateBase);
        painter->setBrush(surface);
        painter->setPen(QPen(selected ? p.color(QPalette::Highlight) : p.color(QPalette::Dark), selected ? 2 : 1));
        painter->drawRoundedRect(card, 14, 14);
        const auto icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        QRect iconRect;
        if (m_recent) {
            iconRect = QRect(card.left() + 14, card.top() + 12, 52, 52);
        } else {
            const QRect imageRect(card.left() + 12, card.top() + 12, card.width() - 24, 146);
            painter->setPen(Qt::NoPen);
            painter->setBrush(p.color(QPalette::Base));
            painter->drawRoundedRect(imageRect, 10, 10);
            iconRect = QRect(imageRect.center().x() - 59, imageRect.center().y() - 59, 118, 118);
        }
        icon.paint(painter, iconRect, Qt::AlignCenter);
        QFont titleFont = option.font;
        titleFont.setPixelSize(m_recent ? 15 : 14);
        titleFont.setWeight(QFont::DemiBold);
        painter->setFont(titleFont);
        painter->setPen(p.color(QPalette::Text));
        const int left = m_recent ? iconRect.right() + 15 : card.left() + 14;
        const int width = m_recent ? qMax(20, playRect(card).left() - left - 14) : card.width() - 28;
        painter->drawText(QRect(left, card.top() + (m_recent ? 15 : 168), width, 22), Qt::AlignVCenter,
                          QFontMetrics(titleFont).elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight, width));
        QFont body = option.font;
        body.setPixelSize(12);
        painter->setFont(body);
        painter->setPen(p.color(QPalette::Mid));
        const auto detail = index.data(kWorkspaceMetadataRole).toString();
        painter->drawText(QRect(left, card.top() + (m_recent ? 40 : 194), width, 20), Qt::AlignVCenter,
                          QFontMetrics(body).elidedText(detail, Qt::ElideRight, width));
        if (!m_recent) {
            const auto state = instance && instance->isRunning() ? tr("Running") : lastPlayedText(instance);
            painter->drawText(QRect(left, card.bottom() - 31, card.width() - 56, 20), Qt::AlignVCenter,
                              QFontMetrics(body).elidedText(state, Qt::ElideRight, card.width() - 56));
        }
        if (m_recent || selected || hover) {
            const auto button = playRect(card);
            painter->setPen(Qt::NoPen);
            painter->setBrush(launchable ? p.color(QPalette::Highlight) : p.color(QPalette::Button));
            painter->drawRoundedRect(button, 8, 8);
            painter->setPen(launchable ? p.color(QPalette::HighlightedText) : p.color(QPalette::Mid));
            QFont buttonFont = option.font;
            buttonFont.setPixelSize(13);
            buttonFont.setWeight(QFont::DemiBold);
            painter->setFont(buttonFont);
            painter->drawText(button, Qt::AlignCenter, instance && instance->isRunning() ? tr("Running") : tr("Play"));
        }
        painter->setFont(body);
        painter->setPen(p.color(QPalette::Mid));
        painter->drawText(moreRect(card), Qt::AlignCenter, QString::fromUtf8("⋮"));
        if (option.state.testFlag(QStyle::State_HasFocus)) {
            painter->setBrush(Qt::NoBrush);
            painter->setPen(QPen(p.color(QPalette::Highlight), 2, Qt::DashLine));
            painter->drawRoundedRect(card.adjusted(3, 3, -3, -3), 11, 11);
        }
        painter->restore();
    }
    bool editorEvent(QEvent* event, QAbstractItemModel*, const QStyleOptionViewItem& option, const QModelIndex& index) override
    {
        if (event->type() != QEvent::MouseButtonRelease) return false;
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() != Qt::LeftButton) return false;
        const QRect card = option.rect.adjusted(3, 3, -5, -5);
        if (moreRect(card).contains(mouse->position().toPoint())) {
            m_context(index, mouse->globalPosition().toPoint());
            return true;
        }
        if (playRect(card).contains(mouse->position().toPoint())) {
            auto* instance = static_cast<BaseInstance*>(index.data(InstanceList::InstancePointerRole).value<void*>());
            if (instance && instance->canLaunch() && !instance->isRunning()) m_activate(index);
            return true;
        }
        return false;
    }
   private:
    bool m_recent;
    Activate m_activate;
    Context m_context;
};

QIcon workspaceIcon(const QString& name) { return QIcon(":/icons/breeze_dark/scalable/" + name + ".svg"); }
QLabel* heading(const QString& text, const char* name, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setObjectName(name);
    return label;
}
} // namespace

WorkspacePage::WorkspacePage(InstanceList* instances, QWidget* parent) : QWidget(parent), m_instances(instances)
{
    initializeWorkspaceResources();
    setObjectName("pxlWorkspace");
    setStyleSheet(QStringLiteral(R"(
        QWidget#pxlWorkspace { background: palette(window); }
        QFrame#workspaceRail { background: palette(base); border-right: 1px solid palette(dark); }
        QFrame#workspaceHeader { border-bottom: 1px solid palette(dark); }
        QScrollArea#workspaceScroll, QWidget#workspaceContent { border: none; background: palette(window); }
        QListView#workspaceCards, QListView#workspaceRecent { border: none; background: transparent; outline: none; }
        QLabel#workspacePageTitle { font-size: 20px; font-weight: 600; }
        QLabel#workspaceSectionTitle { font-size: 20px; font-weight: 600; }
        QLabel#workspaceMuted { color: palette(mid); }
        QLabel#workspaceEmptyTitle { font-size: 22px; font-weight: 600; }
        QLabel#workspaceSelectedName { font-size: 14px; font-weight: 600; }
        QFrame#workspaceSelection { background: palette(base); border-top: 1px solid palette(dark); }
        QPushButton[pxlNav="true"] { border-radius: 12px; padding: 0px; }
    )"));
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    auto* rail = new QFrame(this);
    rail->setObjectName("workspaceRail");
    rail->setFixedWidth(76);
    auto* nav = new QVBoxLayout(rail);
    nav->setContentsMargins(12, 18, 12, 14);
    nav->setSpacing(10);
    auto* logo = new QLabel(rail);
    logo->setPixmap(QIcon(":/pxl/cone-icon.svg").pixmap(40, 40));
    logo->setAlignment(Qt::AlignCenter);
    logo->setToolTip(tr("PXL Cone Launcher"));
    logo->setAccessibleName(tr("PXL Cone Launcher"));
    nav->addWidget(logo);
    nav->addSpacing(20);
    auto navButton = [rail, nav](const char* name, const QString& text, const QString& icon) {
        auto* button = new QPushButton(rail);
        button->setObjectName(name);
        button->setProperty("pxlNav", true);
        button->setIcon(workspaceIcon(icon));
        button->setIconSize(QSize(23, 23));
        button->setFixedSize(50, 46);
        button->setToolTip(text);
        button->setAccessibleName(text);
        nav->addWidget(button);
        return button;
    };
    m_homeButton = navButton("workspaceHome", tr("Home"), "launch");
    m_libraryButton = navButton("workspaceLibrary", tr("Library"), "minecraft");
    m_homeButton->setCheckable(true);
    m_libraryButton->setCheckable(true);
    auto* import = navButton("workspaceImport", tr("Import packs or another launcher"), "copy");
    nav->addStretch();
    m_catPreview = new WorkspaceCatPreview(rail);
    m_catPreview->setMinimumHeight(50);
    m_catPreview->setMaximumHeight(70);
    nav->addWidget(m_catPreview);
    m_catButton = navButton("workspaceCat", tr("Toggle cat"), "appearance");
    auto* classic = navButton("workspaceClassic", tr("Classic view and advanced tools"), "externaltools");
    auto* accounts = navButton("workspaceAccounts", tr("Accounts"), "accounts");
    auto* settings = navButton("workspaceSettings", tr("Settings"), "settings");
    root->addWidget(rail);

    auto* main = new QVBoxLayout;
    main->setContentsMargins(0, 0, 0, 0);
    main->setSpacing(0);
    root->addLayout(main, 1);
    auto* header = new QFrame(this);
    header->setObjectName("workspaceHeader");
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(28, 16, 28, 16);
    m_heading = heading(tr("Home"), "workspacePageTitle", header);
    headerLayout->addWidget(m_heading);
    headerLayout->addStretch();
    auto* brand = heading(tr("PXL Cone"), "workspaceMuted", header);
    headerLayout->addWidget(brand);
    main->addWidget(header);
    m_scroll = new QScrollArea(this);
    m_scroll->setObjectName("workspaceScroll");
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* content = new QWidget;
    content->setObjectName("workspaceContent");
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(28, 24, 24, 24);
    contentLayout->setSpacing(20);
    m_scroll->setWidget(content);
    main->addWidget(m_scroll, 1);

    m_recentSection = new QWidget(content);
    auto* recent = new QVBoxLayout(m_recentSection);
    recent->setContentsMargins(0, 0, 0, 0);
    recent->setSpacing(12);
    recent->addWidget(heading(tr("Continue playing"), "workspaceSectionTitle", m_recentSection));
    m_homeView = new QListView(m_recentSection);
    m_homeView->setObjectName("workspaceRecent");
    m_homeView->setViewMode(QListView::ListMode);
    m_homeView->setAccessibleName(tr("Recently played instances. Press Enter to play."));
    recent->addWidget(m_homeView);
    m_homeEmpty = heading(tr("Your recent games will appear here."), "workspaceMuted", m_recentSection);
    recent->addWidget(m_homeEmpty);
    contentLayout->addWidget(m_recentSection);

    auto* libraryHeading = new QHBoxLayout;
    libraryHeading->setSpacing(10);
    libraryHeading->addWidget(heading(tr("Library"), "workspaceSectionTitle", content));
    m_count = heading(QString(), "workspaceMuted", content);
    libraryHeading->addWidget(m_count);
    libraryHeading->addStretch();
    contentLayout->addLayout(libraryHeading);
    auto* tools = new QHBoxLayout;
    tools->setSpacing(10);
    m_search = new QLineEdit(content);
    m_search->setObjectName("workspaceSearch");
    m_search->setPlaceholderText(tr("Search your library"));
    m_search->setAccessibleName(tr("Search instances"));
    m_search->setClearButtonEnabled(true);
    tools->addWidget(m_search, 1);
    auto* importAction = new QPushButton(tr("Import"), content);
    importAction->setObjectName("workspaceHomeImport");
    importAction->setIcon(workspaceIcon("copy"));
    auto* create = new QPushButton(tr("New instance"), content);
    create->setObjectName("workspaceHomeCreate");
    create->setProperty("pxlPrimary", true);
    create->setIcon(workspaceIcon("new"));
    tools->addWidget(importAction);
    tools->addWidget(create);
    contentLayout->addLayout(tools);
    auto* filterRow = new QHBoxLayout;
    m_groups = new QComboBox(content);
    m_groups->setObjectName("workspaceGroups");
    m_groups->setAccessibleName(tr("Filter by group"));
    m_groups->setMinimumWidth(155);
    filterRow->addWidget(m_groups);
    filterRow->addStretch();
    auto* filterHint = heading(tr("Double-click to play · Right-click for more"), "workspaceMuted", content);
    filterRow->addWidget(filterHint);
    contentLayout->addLayout(filterRow);
    m_libraryView = new QListView(content);
    m_libraryView->setObjectName("workspaceCards");
    m_libraryView->setViewMode(QListView::IconMode);
    m_libraryView->setResizeMode(QListView::Adjust);
    m_libraryView->setWrapping(true);
    m_libraryView->setMovement(QListView::Static);
    m_libraryView->setAccessibleName(tr("Library. Select an instance, then press Enter to play."));
    m_libraryView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    contentLayout->addWidget(m_libraryView);
    m_emptyState = new QWidget(content);
    auto* empty = new QVBoxLayout(m_emptyState);
    empty->setContentsMargins(24, 36, 24, 36);
    empty->setSpacing(12);
    auto* emptyLogo = new QLabel(m_emptyState);
    emptyLogo->setPixmap(QIcon(":/pxl/cone-icon.svg").pixmap(64, 64));
    emptyLogo->setAlignment(Qt::AlignCenter);
    empty->addWidget(emptyLogo);
    m_libraryEmpty = heading(tr("Your library starts here"), "workspaceEmptyTitle", m_emptyState);
    m_libraryEmpty->setAlignment(Qt::AlignCenter);
    empty->addWidget(m_libraryEmpty);
    auto* emptyDetail = heading(tr("Create a Minecraft instance or bring your packs from Prism or Pinecone."), "workspaceMuted", m_emptyState);
    emptyDetail->setWordWrap(true);
    emptyDetail->setAlignment(Qt::AlignCenter);
    empty->addWidget(emptyDetail);
    contentLayout->addWidget(m_emptyState);
    contentLayout->addStretch();

    m_selectionBar = new QFrame(this);
    m_selectionBar->setObjectName("workspaceSelection");
    auto* selected = new QHBoxLayout(m_selectionBar);
    selected->setContentsMargins(28, 12, 28, 12);
    selected->setSpacing(12);
    m_selectedIcon = new QLabel(m_selectionBar);
    selected->addWidget(m_selectedIcon);
    auto* selectedText = new QVBoxLayout;
    selectedText->setSpacing(2);
    m_selectedName = heading(QString(), "workspaceSelectedName", m_selectionBar);
    m_selectedDetail = heading(QString(), "workspaceMuted", m_selectionBar);
    selectedText->addWidget(m_selectedName);
    selectedText->addWidget(m_selectedDetail);
    selected->addLayout(selectedText, 1);
    auto* exportAction = new QPushButton(tr("Share / transfer"), m_selectionBar);
    exportAction->setObjectName("workspaceHomeExport");
    m_moreSelected = new QPushButton(tr("Manage"), m_selectionBar);
    m_moreSelected->setObjectName("workspaceMoreSelected");
    m_playSelected = new QPushButton(tr("Play"), m_selectionBar);
    m_playSelected->setObjectName("workspacePlaySelected");
    m_playSelected->setProperty("pxlPrimary", true);
    m_playSelected->setMinimumWidth(110);
    m_playSelected->setAccessibleName(tr("Play selected instance"));
    selected->addWidget(exportAction);
    selected->addWidget(m_moreSelected);
    selected->addWidget(m_playSelected);
    main->addWidget(m_selectionBar);

    m_recentModel = new WorkspaceInstanceModel(WorkspaceInstanceModel::Mode::Recent, this);
    m_libraryModel = new WorkspaceInstanceModel(WorkspaceInstanceModel::Mode::Library, this);
    m_recentModel->setSourceModel(instances);
    m_libraryModel->setSourceModel(instances);
    m_homeView->setModel(m_recentModel);
    m_libraryView->setModel(m_libraryModel);
    for (auto* view : {m_homeView, m_libraryView}) {
        view->setUniformItemSizes(true);
        view->setSelectionMode(QAbstractItemView::SingleSelection);
        view->setEditTriggers(QAbstractItemView::NoEditTriggers);
        view->setMouseTracking(true);
        view->setContextMenuPolicy(Qt::CustomContextMenu);
        view->setItemDelegate(new WorkspaceCardDelegate(view == m_homeView,
            [this](const QModelIndex& index) { emit activateInstance(idForIndex(index)); },
            [this](const QModelIndex& index, const QPoint& point) { emit contextMenuRequested(idForIndex(index), point); }, view));
        connect(view, &QListView::activated, this, [this](const QModelIndex& index) {
            auto* instance = m_instances->getInstanceById(idForIndex(index));
            if (instance && instance->canLaunch() && !instance->isRunning()) emit activateInstance(instance->id());
        });
        connect(view->selectionModel(), &QItemSelectionModel::currentChanged, this,
                [this, view](const QModelIndex& current) { syncSelection(view, current); });
        connect(view, &QWidget::customContextMenuRequested, this, [this, view](const QPoint& point) { showContextMenu(view, point); });
    }
    m_libraryView->viewport()->installEventFilter(this);
    connect(m_homeButton, &QPushButton::clicked, this, [this] { showHome(true); });
    connect(m_libraryButton, &QPushButton::clicked, this, [this] { showHome(false); });
    connect(import, &QPushButton::clicked, this, &WorkspacePage::importRequested);
    connect(importAction, &QPushButton::clicked, this, &WorkspacePage::importRequested);
    connect(create, &QPushButton::clicked, this, &WorkspacePage::createRequested);
    connect(exportAction, &QPushButton::clicked, this, &WorkspacePage::exportRequested);
    connect(accounts, &QPushButton::clicked, this, &WorkspacePage::accountsRequested);
    connect(settings, &QPushButton::clicked, this, &WorkspacePage::settingsRequested);
    connect(classic, &QPushButton::clicked, this, &WorkspacePage::classicRequested);
    connect(m_catButton, &QPushButton::clicked, this, &WorkspacePage::catRequested);
    connect(m_playSelected, &QPushButton::clicked, this, [this] {
        if (m_playSelected->isEnabled() && !m_selectedId.isEmpty()) emit activateInstance(m_selectedId);
    });
    connect(m_moreSelected, &QPushButton::clicked, this, [this] {
        if (!m_selectedId.isEmpty()) emit contextMenuRequested(m_selectedId, m_moreSelected->mapToGlobal(QPoint(0, m_moreSelected->height())));
    });
    auto filtered = [this] { updateLayout(); setSelectedInstance(m_selectedId); };
    connect(m_search, &QLineEdit::textChanged, this, [this, filtered](const QString& text) {
        static_cast<WorkspaceInstanceModel*>(m_libraryModel)->setSearchText(text);
        filtered();
    });
    connect(m_groups, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, filtered](int) {
        static_cast<WorkspaceInstanceModel*>(m_libraryModel)->setGroup(m_groups->currentData().toString());
        filtered();
    });
    for (auto* model : {m_recentModel, m_libraryModel}) {
        connect(model, &QAbstractItemModel::modelReset, this, [this] { QTimer::singleShot(0, this, &WorkspacePage::updateLayout); });
        connect(model, &QAbstractItemModel::rowsInserted, this, [this] { QTimer::singleShot(0, this, &WorkspacePage::updateLayout); });
        connect(model, &QAbstractItemModel::rowsRemoved, this, [this] { QTimer::singleShot(0, this, &WorkspacePage::updateLayout); });
    }
    if (m_instances) {
        connect(m_instances, &InstanceList::instancesChanged, this, &WorkspacePage::refresh);
        connect(m_instances, &InstanceList::groupsChanged, this, [this] { updateGroups(); updateLayout(); });
        connect(m_instances, &QAbstractItemModel::modelReset, this, &WorkspacePage::refresh);
        connect(m_instances, &QAbstractItemModel::dataChanged, this, &WorkspacePage::refresh);
    }
    if (auto* app = APPLICATION_DYN; app && app->icons()) {
        connect(app->icons(), &IconList::iconUpdated, this, [this] {
            m_homeView->viewport()->update();
            m_libraryView->viewport()->update();
        });
    }
    refresh();
    showHome(true);
}

void WorkspacePage::showHome(bool home)
{
    m_showHome = home;
    m_heading->setText(home ? tr("Home") : tr("Library"));
    m_homeButton->setChecked(home);
    m_libraryButton->setChecked(!home);
    updateLayout();
    m_scroll->verticalScrollBar()->setValue(0);
}

void WorkspacePage::updateLayout()
{
    const int count = m_libraryModel->rowCount();
    const int total = m_instances ? m_instances->rowCount() : 0;
    m_count->setText(count == total ? tr("%n instance(s)", nullptr, total) : tr("%1 of %2").arg(count).arg(total));
    m_recentSection->setVisible(m_showHome && m_recentModel->rowCount() > 0);
    m_homeView->setFixedHeight(qMin(3, m_recentModel->rowCount()) * 84 + 4);
    m_homeEmpty->setVisible(false);
    m_emptyState->setVisible(count == 0);
    m_libraryEmpty->setText(total == 0 ? tr("Your library starts here") : tr("No matching instances"));
    m_libraryView->setVisible(count > 0);
    const int available = qMax(190, m_libraryView->viewport()->width());
    const int columns = qMax(1, available / 198);
    m_libraryView->setGridSize(QSize(available / columns, 250));
    const int rows = (count + columns - 1) / columns;
    m_libraryView->setFixedHeight(qMax(1, qMin(rows, 5)) * 250 + 8);
}

void WorkspacePage::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    QTimer::singleShot(0, this, &WorkspacePage::updateLayout);
}

bool WorkspacePage::eventFilter(QObject* object, QEvent* event)
{
    if (object == m_libraryView->viewport() && event->type() == QEvent::Resize)
        QTimer::singleShot(0, this, &WorkspacePage::updateLayout);
    return QWidget::eventFilter(object, event);
}

void WorkspacePage::setSelectedInstance(const QString& id)
{
    m_selectedId = id;
    m_syncingSelection = true;
    selectInView(m_homeView, id);
    selectInView(m_libraryView, id);
    m_syncingSelection = false;
    const auto* instance = m_instances ? m_instances->getInstanceById(id) : nullptr;
    const bool visible = !id.isEmpty() && idForIndex(m_libraryView->currentIndex()) == id;
    m_playSelected->setEnabled(visible && instance && instance->canLaunch() && !instance->isRunning());
    m_moreSelected->setEnabled(visible && instance != nullptr);
    m_selectionBar->setVisible(visible && instance != nullptr);
    if (visible && instance) {
        const auto index = m_libraryView->currentIndex();
        m_selectedName->setText(instance->name());
        m_selectedDetail->setText(index.data(kWorkspaceMetadataRole).toString());
        m_selectedIcon->setPixmap(qvariant_cast<QIcon>(index.data(Qt::DecorationRole)).pixmap(36, 36));
        m_playSelected->setText(instance->isRunning() ? tr("Running") : tr("Play"));
    }
}

void WorkspacePage::refresh()
{
    updateGroups();
    static_cast<WorkspaceInstanceModel*>(m_recentModel)->rebuildMetadata();
    static_cast<WorkspaceInstanceModel*>(m_libraryModel)->rebuildMetadata();
    m_recentModel->sort(0);
    m_libraryModel->sort(0);
    updateHome();
    updateLayout();
    setSelectedInstance(m_selectedId);
    m_libraryView->viewport()->update();
    m_homeView->viewport()->update();
}

void WorkspacePage::setPaintCat(bool visible) { m_catPreview->setPaintCat(visible); }
void WorkspacePage::setCatButtonVisible(bool visible) { m_catButton->setVisible(visible); }
QString WorkspacePage::idForIndex(const QModelIndex& index) const
{
    return index.isValid() ? index.data(InstanceList::InstanceIDRole).toString() : QString();
}

void WorkspacePage::selectInView(QListView* view, const QString& id)
{
    if (!view || !view->model()) return;
    for (int row = 0; row < view->model()->rowCount(); ++row) {
        const auto index = view->model()->index(row, 0);
        if (!id.isEmpty() && idForIndex(index) == id) {
            if (view->currentIndex() != index) view->setCurrentIndex(index);
            return;
        }
    }
    view->selectionModel()->clear();
}

void WorkspacePage::syncSelection(QListView*, const QModelIndex& current)
{
    if (m_syncingSelection || !current.isValid()) return;
    const auto id = idForIndex(current);
    if (id.isEmpty() || id == m_selectedId) return;
    setSelectedInstance(id);
    emit selectInstance(id);
}

void WorkspacePage::updateGroups()
{
    const QString previous = m_groups->currentData().toString();
    m_groups->blockSignals(true);
    m_groups->clear();
    m_groups->addItem(tr("All groups"), QLatin1String(kAllGroups));
    if (m_instances) {
        auto groups = m_instances->getGroups();
        std::sort(groups.begin(), groups.end(), [](const QString& lhs, const QString& rhs) {
            return QString::localeAwareCompare(lhs, rhs) < 0;
        });
        for (const auto& group : groups) m_groups->addItem(group, group);
    }
    const int index = m_groups->findData(previous);
    m_groups->setCurrentIndex(index >= 0 ? index : 0);
    m_groups->blockSignals(false);
    static_cast<WorkspaceInstanceModel*>(m_libraryModel)->setGroup(m_groups->currentData().toString());
}

void WorkspacePage::updateHome() { updateLayout(); }

void WorkspacePage::showContextMenu(QListView* view, const QPoint& position)
{
    const auto id = idForIndex(view->indexAt(position));
    if (!id.isEmpty()) emit contextMenuRequested(id, view->viewport()->mapToGlobal(position));
}

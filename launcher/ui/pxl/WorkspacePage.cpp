// SPDX-License-Identifier: GPL-3.0-only

#include "WorkspacePage.h"
#include "Animations.h"
#include "TaskHistory.h"
#include "Backdrop.h"
#include "LibraryControls.h"

#include <algorithm>

#include <Application.h>
#include <BaseInstance.h>
#include <InstanceList.h>
#include <icons/IconList.h>
#include <minecraft/MinecraftInstance.h>
#include <minecraft/PackProfile.h>
#include <minecraft/Component.h>
#include <modplatform/ModIndex.h>
#include <ui/themes/CatPainter.h>
#include <ui/themes/ThemeManager.h>

#include <QApplication>
#include <QGraphicsBlurEffect>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <settings/SettingsObject.h>
#include <QDateTime>
#include <QMouseEvent>
#include <QCursor>
#include <QKeyEvent>
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
#include <QPainterPath>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QVBoxLayout>
#include "Icons.h"

static void initializeWorkspaceResources()
{
    static const bool initialized = [] {
        Q_INIT_RESOURCE(fonts);
        Q_INIT_RESOURCE(breeze_dark);
        Q_INIT_RESOURCE(multimc);
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

class WorkspaceBanner final : public QWidget {
   public:
    explicit WorkspaceBanner(QWidget* parent) : QWidget(parent), m_image(":/pxl/cone-dawn-banner.png")
    {
        setFixedHeight(145);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
   protected:
    void paintEvent(QPaintEvent*) override
    {
        if (m_image.isNull()) return;
        QPainter painter(this);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        QPainterPath clip;
        clip.addRoundedRect(rect(), 12, 12);
        painter.setClipPath(clip);
        const auto scale = qMax(qreal(width()) / m_image.width(), qreal(height()) / m_image.height());
        const QSizeF size(m_image.width() * scale, m_image.height() * scale);
        painter.drawPixmap(QRectF((width() - size.width()) / 2, (height() - size.height()) / 2, size.width(), size.height()),
                           m_image, QRectF(m_image.rect()));
    }
   private:
    QPixmap m_image;
};

class WorkspaceHero final : public QFrame {
   public:
    explicit WorkspaceHero(QWidget* parent) : QFrame(parent), m_image(":/pxl/wallpaper-source.png")
    {
        setMinimumHeight(220);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    void setArtworkEnabled(bool enabled) { m_artwork = enabled; update(); }
   protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        QPainterPath clip;
        clip.addRoundedRect(QRectF(rect()), 16, 16);
        painter.setClipPath(clip);
        painter.fillRect(rect(), palette().color(QPalette::Base));
        if (!m_artwork || m_image.isNull()) return;
        const auto scale = qMax(qreal(width()) / m_image.width(), qreal(height()) / m_image.height());
        const QSizeF size(m_image.width() * scale, m_image.height() * scale);
        painter.drawPixmap(QRectF((width() - size.width()) / 2, (height() - size.height()) / 2, size.width(), size.height()),
                           m_image, QRectF(m_image.rect()));
        QLinearGradient shade(0, 0, width(), 0);
        shade.setColorAt(0, QColor(12, 17, 21, 245));
        shade.setColorAt(0.55, QColor(12, 17, 21, 180));
        shade.setColorAt(1, QColor(12, 17, 21, 30));
        painter.fillRect(rect(), shade);
    }
   private:
    QPixmap m_image;
    bool m_artwork = true;
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

    void setExcludedId(const QString& id)
    {
        if (id == m_excludedId) return;
        m_excludedId = id;
        invalidateFilter();
    }

    void setRecentOrder(bool recent)
    {
        m_recentOrder = recent;
        invalidate();
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
                    auto minecraftVersion = profile->getComponentVersion("net.minecraft");
                    if (minecraftVersion.isEmpty()) {
                        if (const auto component = profile->getComponent("net.minecraft")) minecraftVersion = component->m_version;
                    }
                    const auto loaders = profile->getModLoadersList();
                    QStringList loaderNames;
                    for (const auto loader : loaders) {
                        auto name = ModPlatform::getModLoaderAsString(loader);
                        if (!name.isEmpty()) name[0] = name[0].toUpper();
                        loaderNames.append(name);
                    }
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
            const QIcon fallback(":/icons/multimc/scalable/instances/" + key + ".svg");
            return fallback.isNull() ? QIcon(":/icons/breeze_dark/scalable/minecraft.svg") : fallback;
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
        if (!m_excludedId.isEmpty() && index.data(InstanceList::InstanceIDRole).toString() == m_excludedId)
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
        if ((m_mode == Mode::Recent || m_recentOrder) && lhs && rhs && lhs->lastLaunch() != rhs->lastLaunch())
            return lhs->lastLaunch() > rhs->lastLaunch();
        return QString::localeAwareCompare(left.data(Qt::DisplayRole).toString(), right.data(Qt::DisplayRole).toString()) < 0;
    }

   private:
    Mode m_mode;
    QString m_search;
    QString m_excludedId;
    bool m_recentOrder = false;
    QString m_group = QLatin1String(kAllGroups);
    QHash<QString, QString> m_metadata;
};

QString lastPlayedText(const BaseInstance* instance)
{
    if (!instance || instance->lastLaunch() <= 0)
        return QObject::tr("Not played yet");
    const auto seconds = qMax<qint64>(0, (QDateTime::currentMSecsSinceEpoch() - instance->lastLaunch()) / 1000);
    if (seconds < 60) return QObject::tr("Played just now");
    if (seconds < 3600) return QObject::tr("Played %1 min ago").arg(seconds / 60);
    if (seconds < 86400) return QObject::tr("Played %1 h ago").arg(seconds / 3600);
    return QObject::tr("Played %1 d ago").arg(seconds / 86400);
}

QString playedDuration(BaseInstance* instance)
{
    if (!instance || !instance->isGameTimeShown()) return {};
    const qint64 minutes = qMax<qint64>(0, instance->totalTimePlayed()) / 60;
    if (!instance->isGameTimeRecordingEnabled() && minutes == 0) return QObject::tr("Not recorded");
    return minutes >= 60 ? QObject::tr("%1 h %2 min played").arg(minutes / 60).arg(minutes % 60)
                         : QObject::tr("%1 min played").arg(minutes);
}

class WorkspaceCardDelegate final : public QStyledItemDelegate {
   public:
    using Activate = std::function<void(const QModelIndex&)>;
    using Context = std::function<void(const QModelIndex&, const QPoint&)>;
    WorkspaceCardDelegate(Activate activate, Activate stop, Context context, QObject* parent)
        : QStyledItemDelegate(parent), m_activate(std::move(activate)), m_stop(std::move(stop)), m_context(std::move(context)) {}
    bool cards() const { return parent()->property("pxlCards").toBool(); }
    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override
    {
        if (!cards()) return {400, 76};
        const auto* view = qobject_cast<QListView*>(parent());
        return {view && view->gridSize().width() > 0 ? view->gridSize().width() : 220, 260};
    }
    QRect playRect(const QRect& row) const { return cards() ? QRect(row.right() - 52, row.bottom() - 54, 36, 36) : QRect(row.right() - 104, row.center().y() - 22, 44, 44); }
    QRect moreRect(const QRect& row) const { return cards() ? QRect(row.right() - 40, row.top() + 10, 30, 30) : QRect(row.right() - 52, row.center().y() - 22, 44, 44); }
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        if (cards()) { paintTile(painter, option, index); return; }
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const auto palette = QApplication::palette();
        const QRect row = option.rect.adjusted(0, 2, -4, -2);
        const bool selected = option.state.testFlag(QStyle::State_Selected);
        const bool hover = option.state.testFlag(QStyle::State_MouseOver);
        painter->setPen(Qt::NoPen);
        QColor surface = palette.color(QPalette::Base);
        if (selected) surface = surface.lighter(110);
        if (hover && !selected) surface = palette.color(QPalette::AlternateBase);
        surface.setAlpha(190);
        painter->setBrush(surface);
        painter->drawRoundedRect(row, 10, 10);
        if (selected) {
            painter->setBrush(palette.color(QPalette::Mid));
            painter->drawRoundedRect(QRect(row.left() + 1, row.top() + 18, 3, row.height() - 36), 1.5, 1.5);
        }
        const auto icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        icon.paint(painter, QRect(row.left() + 12, row.center().y() - 22, 44, 44), Qt::AlignCenter);
        auto* instance = static_cast<BaseInstance*>(index.data(InstanceList::InstancePointerRole).value<void*>());
        const bool wide = row.width() >= 680;
        const int textLeft = row.left() + 72;
        const int textWidth = qMax(40, row.width() - (wide ? 382 : 190));
        QFont title = option.font;
        title.setPixelSize(16);
        title.setWeight(QFont::Bold);
        painter->setFont(title);
        painter->setPen(palette.color(QPalette::Text));
        painter->drawText(QRect(textLeft, row.top() + 11, textWidth, 23), Qt::AlignVCenter,
                          QFontMetrics(title).elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight, textWidth));
        QFont body = option.font;
        body.setPixelSize(13);
        body.setWeight(QFont::Medium);
        painter->setFont(body);
        painter->setPen(palette.color(QPalette::Mid));
        const auto group = index.data(InstanceList::GroupRole).toString();
        const int badgeWidth = group.isEmpty() ? 0 : qMin(textWidth, qMin(120, QFontMetrics(body).horizontalAdvance(group) + 16));
        const int metadataWidth = qMax(0, textWidth - (badgeWidth ? badgeWidth + 8 : 0));
        painter->drawText(QRect(textLeft, row.top() + 38, metadataWidth, 20), Qt::AlignVCenter,
                          QFontMetrics(body).elidedText(index.data(kWorkspaceMetadataRole).toString(), Qt::ElideRight, metadataWidth));
        if (badgeWidth) paintGroupBadge(painter, QRect(textLeft + textWidth - badgeWidth, row.top() + 38, badgeWidth, 20), group);
        const auto state = instance && instance->isRunning() ? tr("Running")
                         : (instance && !instance->canLaunch() ? tr("Needs attention") : lastPlayedText(instance));
        if (wide) {
            const auto duration = playedDuration(instance);
            painter->drawText(QRect(row.right() - 286, row.top() + (duration.isEmpty() ? 0 : 10), 166, duration.isEmpty() ? row.height() : 22), Qt::AlignRight | Qt::AlignVCenter, state);
            if (!duration.isEmpty()) painter->drawText(QRect(row.right() - 286, row.top() + 37, 166, 22), Qt::AlignRight | Qt::AlignVCenter, duration);
        }
        const bool running = instance && instance->isRunning();
        const bool launchable = instance && (instance->canLaunch() || running);
        painter->setPen(launchable ? palette.color(QPalette::Text) : palette.color(QPalette::Mid));
        painter->setPen(Qt::NoPen);
        painter->setBrush(running ? QColor("#673b3c") : QColor(255, 238, 218, 15));
        painter->drawRoundedRect(playRect(row), 8, 8);
        static const QIcon playIcon(":/pxl/icons/play.svg");
        static const QIcon stopIcon(":/pxl/icons/stop.svg");
        static const QIcon moreIcon(":/pxl/icons/dots-three-vertical.svg");
        (running ? stopIcon : playIcon).paint(painter, playRect(row).adjusted(12, 12, -12, -12), Qt::AlignCenter,
                       launchable ? QIcon::Normal : QIcon::Disabled);
        moreIcon.paint(painter, moreRect(row).adjusted(12, 12, -12, -12), Qt::AlignCenter);
        if (option.state.testFlag(QStyle::State_HasFocus)) {
            painter->setBrush(Qt::NoBrush);
            QColor focus = palette.color(QPalette::Mid);
            focus.setAlpha(90);
            painter->setPen(QPen(focus, 1));
            painter->drawRoundedRect(row.adjusted(1, 1, -1, -1), 5, 5);
        }
        painter->restore();
    }
    bool editorEvent(QEvent* event, QAbstractItemModel*, const QStyleOptionViewItem& option, const QModelIndex& index) override
    {
        if (event->type() != QEvent::MouseButtonRelease) return false;
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() != Qt::LeftButton) return false;
        const auto row = option.rect.adjusted(0, 2, -4, -2);
        if (moreRect(row).contains(mouse->position().toPoint())) {
            m_context(index, mouse->globalPosition().toPoint());
            return true;
        }
        if (playRect(row).contains(mouse->position().toPoint())) {
            auto* instance = static_cast<BaseInstance*>(index.data(InstanceList::InstancePointerRole).value<void*>());
            if (instance && instance->isRunning()) m_stop(index);
            else if (instance && instance->canLaunch()) m_activate(index);
            return true;
        }
        return false;
    }
   private:
    void paintGroupBadge(QPainter* painter, const QRect& area, const QString& group) const
    {
        if (group.isEmpty() || area.width() < 16) return;
        painter->save();
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(238, 226, 211, 17));
        painter->drawRoundedRect(area, 7, 7);
        QFont font = painter->font();
        font.setPixelSize(11);
        font.setWeight(QFont::DemiBold);
        painter->setFont(font);
        painter->setPen(QColor("#c5bdba"));
        painter->drawText(area.adjusted(8, 0, -8, 0), Qt::AlignVCenter,
            QFontMetrics(font).elidedText(group, Qt::ElideRight, area.width() - 16));
        painter->restore();
    }
    void paintTile(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const auto palette = QApplication::palette();
        const QRect row = option.rect.adjusted(0, 2, -4, -2);
        const bool selected = option.state.testFlag(QStyle::State_Selected);
        const bool hover = option.state.testFlag(QStyle::State_MouseOver);
        painter->setPen(selected ? QColor(242, 163, 101, 100) : QColor(165, 176, 193, 26));
        // The workspace wallpaper is already blurred and cached. Blend the card
        // into that material, without capturing our own text during painting.
        painter->setBrush(hover ? QColor(47, 49, 55, 210) : QColor(32, 35, 41, 180));
        painter->drawRoundedRect(row, 18, 18);
        const auto icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        icon.paint(painter, QRect(row.left() + 16, row.top() + 16, 96, 96), Qt::AlignCenter);
        const auto group = index.data(InstanceList::GroupRole).toString();
        paintGroupBadge(painter, QRect(row.left() + 124, row.top() + 66, row.width() - 140, 22), group);
        QFont title = option.font;
        title.setPixelSize(16);
        title.setWeight(QFont::Bold);
        painter->setFont(title);
        painter->setPen(palette.color(QPalette::Text));
        painter->drawText(QRect(row.left() + 16, row.top() + 124, row.width() - 32, 24), Qt::AlignVCenter,
            QFontMetrics(title).elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight, row.width() - 32));
        QFont body = option.font;
        body.setPixelSize(12);
        body.setWeight(QFont::Medium);
        painter->setFont(body);
        painter->setPen(palette.color(QPalette::Mid));
        painter->drawText(QRect(row.left() + 16, row.top() + 154, row.width() - 32, 20), Qt::AlignVCenter,
            QFontMetrics(body).elidedText(index.data(kWorkspaceMetadataRole).toString(), Qt::ElideRight, row.width() - 32));
        auto* instance = static_cast<BaseInstance*>(index.data(InstanceList::InstancePointerRole).value<void*>());
        const auto state = instance && instance->isRunning() ? tr("Running") : lastPlayedText(instance);
        const int footerWidth = qMax(20, row.width() - 76);
        painter->drawText(QRect(row.left() + 16, row.top() + 182, row.width() - 32, 18), Qt::AlignVCenter,
            QFontMetrics(body).elidedText(state, Qt::ElideRight, row.width() - 32));
        painter->drawText(QRect(row.left() + 16, playRect(row).top(), footerWidth, 36), Qt::AlignVCenter,
            QFontMetrics(body).elidedText(playedDuration(instance), Qt::ElideRight, footerWidth));
        const bool running = instance && instance->isRunning();
        const bool launchable = instance && (instance->canLaunch() || running);
        const auto* view = qobject_cast<QListView*>(parent());
        const bool overPlay = hover && view && playRect(row).contains(view->viewport()->mapFromGlobal(QCursor::pos()));
        painter->setBrush(running ? (overPlay ? QColor("#a94b50") : QColor("#673b3c"))
            : launchable ? (overPlay ? QColor("#f2a365") : QColor(242, 163, 101, 30)) : QColor(255, 238, 218, 18));
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(playRect(row), 9, 9);
        const auto actionIcon = running ? QIcon(":/pxl/icons/stop.svg") : launchable && overPlay ? Pxl::primaryIcon("play") : QIcon(":/pxl/icons/play.svg");
        actionIcon.paint(painter, playRect(row).adjusted(9, 9, -9, -9), Qt::AlignCenter,
                         launchable ? QIcon::Normal : QIcon::Disabled);
        QIcon(":/pxl/icons/dots-three-vertical.svg").paint(painter, moreRect(row).adjusted(6, 6, -6, -6));
        painter->restore();
    }
    Activate m_activate;
    Activate m_stop;
    Context m_context;
};

QIcon workspaceIcon(const QString& name)
{
    static const QHash<QString, QString> names{{"launch", "house"}, {"minecraft", "squares-four"}, {"copy", "import"},
        {"accounts", "user-circle"}, {"settings", "gear-six"}, {"appearance", "cat"}, {"externaltools", "sliders-horizontal"},
        {"new", "plus"}, {"export", "share-network"}};
    return QIcon(":/pxl/icons/" + names.value(name, name) + ".svg");
}
QLabel* heading(const QString& text, const char* name, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setObjectName(name);
    return label;
}
} // namespace

WorkspacePage::WorkspacePage(InstanceList* instances, QWidget* parent, SettingsObject* preferences)
    : QWidget(parent), m_instances(instances), m_preferences(preferences)
{
    if (auto* app = APPLICATION_DYN; app && !m_preferences) m_preferences = app->settings();
    initializeWorkspaceResources();
    setObjectName("pxlWorkspace");
    setStyleSheet(QStringLiteral(R"(
        QWidget#pxlWorkspace { background: palette(window); }
        QFrame#workspaceRail { background: palette(base); border-right: 1px solid palette(dark); }
        QWidget#pxlWorkspace[pxlTopNavigation="false"] QFrame#workspaceRail {
            background: rgba(32, 35, 40, 210); border-right: 1px solid rgba(164, 173, 186, 24);
        }
        QFrame#workspaceHeader { border-bottom: 1px solid palette(dark); }
        QScrollArea#workspaceScroll, QWidget#workspaceContent { border: none; background: transparent; }
        QListView#workspaceCards, QListView#workspaceRecent { border: none; background: transparent; outline: none; }
        QLabel#workspacePageTitle { font-size: 20px; font-weight: 700; }
        QLabel#workspaceSectionTitle { font-size: 20px; font-weight: 700; }
        QLabel#workspaceMuted { color: palette(mid); }
        QLabel#workspaceEmptyTitle { font-size: 22px; font-weight: 600; }
        QLabel#workspaceFeatureTitle { font-size: 30px; font-weight: 700; }
        QPushButton[pxlNav="true"] { border-radius: 6px; padding: 8px 12px; text-align: left; }
        QWidget#pxlWorkspace[pxlTopNavigation="false"] QPushButton[pxlNav="true"] {
            padding: 0px; text-align: center; border: 1px solid transparent; border-radius: 12px;
        }
        QWidget#pxlWorkspace[pxlTopNavigation="false"] QPushButton[pxlNav="true"]:checked {
            background: palette(button); border: 1px solid palette(highlight);
        }
        QWidget#pxlWorkspace[pxlTopNavigation="true"] QPushButton[pxlNav="true"]:checked {
            background: palette(button); border: 1px solid transparent; border-radius: 10px; font-weight: 600;
        }
        QWidget#pxlWorkspace[pxlTopNavigation="true"] QPushButton[pxlNav="true"]:checked:hover {
            background: palette(button); border-color: palette(dark);
        }
        QWidget#pxlWorkspace[pxlTopNavigation="true"] QPushButton[pxlNav="true"]:focus {
            border: 1px solid palette(highlight);
        }
    )"));
    auto* root = new QHBoxLayout(this);
    m_rootLayout = root;
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    auto* rail = new QFrame(this);
    m_navigation = rail;
    rail->setObjectName("workspaceRail");
    rail->setFixedWidth(184);
    auto* nav = new QVBoxLayout(rail);
    m_navigationLayout = nav;
    nav->setContentsMargins(12, 18, 12, 14);
    nav->setSpacing(10);
    auto* logo = new QLabel(rail);
    logo->setObjectName("workspaceBrandIcon");
    logo->setPixmap(QIcon(":/pxl/cone-icon.svg").pixmap(40, 40));
    logo->setAlignment(Qt::AlignCenter);
    logo->setToolTip(tr("PXL Cone Launcher"));
    logo->setAccessibleName(tr("PXL Cone Launcher"));
    auto* brandRow = new QHBoxLayout;
    brandRow->setSpacing(8);
    brandRow->addWidget(logo);
    auto* brandName = new QLabel(tr("PXL Cone"), rail);
    brandName->setObjectName("workspaceBrandName");
    brandName->setStyleSheet("font-size: 17px; font-weight: 700;");
    brandRow->addWidget(brandName, 1);
    nav->addLayout(brandRow);
    nav->addSpacing(20);
    auto navButton = [this, rail, nav](const char* name, const QString& text, const QString& icon) {
        auto* button = new QPushButton(text, rail);
        button->setObjectName(name);
        button->setProperty("pxlNav", true);
        button->setIcon(workspaceIcon(icon));
        button->setIconSize(QSize(20, 20));
        button->setFixedHeight(42);
        button->setToolTip(text);
        button->setAccessibleName(text);
        nav->addWidget(button);
        m_navigationButtons.append(button);
        return button;
    };
    m_homeButton = navButton("workspaceHome", tr("Home"), "launch");
    m_libraryButton = navButton("workspaceLibrary", tr("Library"), "minecraft");
    m_homeButton->setCheckable(true);
    m_libraryButton->setCheckable(true);
    auto* import = navButton("workspaceImport", tr("Import packs or another launcher"), "copy");
    import->setText(tr("Import"));
    auto* downloads = navButton("workspaceDownloads", tr("Downloads & tasks"), "download-simple");
    connect(downloads, &QPushButton::clicked, this, [downloads] { Pxl::TaskHistory::instance()->showTasks(downloads); });
    connect(Pxl::TaskHistory::instance(), &Pxl::TaskHistory::changed, this, [downloads] {
        const int active = Pxl::TaskHistory::instance()->activeCount();
        downloads->setProperty("pxlBusy", active > 0);
        downloads->setToolTip(tr("Downloads & tasks") + (active > 0 ? QString(" · %1").arg(active) : QString()));
        downloads->style()->unpolish(downloads);
        downloads->style()->polish(downloads);
    });
    nav->addStretch();
    m_catPreview = new WorkspaceCatPreview(this);
    m_catPreview->setMinimumHeight(50);
    m_catPreview->setMaximumHeight(70);
    m_catPreview->hide();
    m_catButton = navButton("workspaceCat", tr("Toggle cat"), "appearance");
    m_catButton->setText(tr("Cat"));
    auto* classic = navButton("workspaceClassic", tr("Classic view and advanced tools"), "externaltools");
    classic->setText(tr("Advanced"));
    classic->hide();
    auto* accounts = navButton("workspaceAccounts", tr("Accounts"), "accounts");
    auto* settings = navButton("workspaceSettings", tr("Settings"), "settings");
    settings->setCheckable(true);
    for (auto* button : m_navigationButtons) button->setProperty("pxlNavLabel", button->text());
    root->addWidget(rail);

    auto* main = new QVBoxLayout;
    main->setContentsMargins(0, 0, 0, 0);
    main->setSpacing(0);
    root->addLayout(main, 1);
    auto* header = new QFrame(this);
    m_header = header;
    header->setObjectName("workspaceHeader");
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(28, 16, 28, 16);
    m_backButton = new QPushButton(tr("Back"), header);
    m_backButton->setObjectName("workspaceBack");
    m_backButton->setIcon(QIcon(":/pxl/icons/arrow-left.svg"));
    m_backButton->hide();
    connect(m_backButton, &QPushButton::clicked, this, [this] { leaveInstanceView(); });
    headerLayout->addWidget(m_backButton);
    m_heading = heading(tr("Home"), "workspacePageTitle", header);
    headerLayout->addWidget(m_heading);
    headerLayout->addStretch();
    main->addWidget(header);
    m_scroll = new QScrollArea(this);
    m_scroll->setObjectName("workspaceScroll");
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->viewport()->setAutoFillBackground(false);
    auto* content = new QWidget;
    content->setObjectName("workspaceContent");
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(28, 24, 24, 24);
    contentLayout->setSpacing(20);
    m_scroll->setWidget(content);
    auto* workspaceBody = new QHBoxLayout;
    workspaceBody->setSpacing(0);
    m_contentStack = new QStackedWidget(this);
    m_contentStack->addWidget(m_scroll);
    workspaceBody->addWidget(m_contentStack, 1);
    main->addLayout(workspaceBody, 1);

    m_recentSection = new WorkspaceHero(content);
    auto* recent = new QVBoxLayout(m_recentSection);
    recent->setContentsMargins(28, 24, 28, 24);
    recent->setSpacing(8);
    m_featuredTime = heading(QString(), "workspaceMuted", m_recentSection);
    m_featuredName = heading(QString(), "workspaceFeatureTitle", m_recentSection);
    m_featuredName->setWordWrap(true);
    m_featuredDetail = heading(QString(), "workspaceMuted", m_recentSection);
    recent->addWidget(m_featuredTime);
    recent->addWidget(m_featuredName);
    recent->addWidget(m_featuredDetail);
    recent->addStretch();
    auto* featureActions = new QHBoxLayout;
    featureActions->setSpacing(12);
    m_featuredPlay = new QPushButton(tr("Play"), m_recentSection);
    m_featuredPlay->setObjectName("workspaceContinue");
    m_featuredPlay->setProperty("pxlPrimary", true);
    m_featuredPlay->setIconSize(QSize(20, 20));
    m_featuredPlay->setFixedSize(132, 42);
    m_featuredPlay->setAccessibleName(tr("Play last used instance"));
    auto* manageFeatured = new QPushButton(tr("Configure"), m_recentSection);
    manageFeatured->setObjectName("workspaceConfigureFeatured");
    manageFeatured->setProperty("pxlQuiet", true);
    featureActions->addWidget(m_featuredPlay);
    featureActions->addWidget(manageFeatured);
    auto* browseMods = new QPushButton(tr("Find content"), m_recentSection);
    browseMods->setObjectName("workspaceBrowseMods");
    browseMods->setProperty("pxlQuiet", true);
    browseMods->setIcon(workspaceIcon("squares-four"));
    featureActions->addWidget(browseMods);
    connect(browseMods, &QPushButton::clicked, this, [this] {
        if (!m_featuredId.isEmpty()) emit browseModsRequested(m_featuredId);
    });
    featureActions->addStretch();
    recent->addLayout(featureActions);
    contentLayout->addWidget(m_recentSection);
    // The recent proxy stays authoritative for choosing the featured instance.
    // Its hidden view keeps the same selection synchronisation as the Library.
    m_homeView = new QListView(this);
    m_homeView->setObjectName("workspaceRecent");
    m_homeView->hide();

    auto* libraryHeading = new QHBoxLayout;
    libraryHeading->setSpacing(10);
    m_libraryTitle = heading(tr("Other instances"), "workspaceSectionTitle", content);
    libraryHeading->addWidget(m_libraryTitle);
    m_count = heading(QString(), "workspaceMuted", content);
    libraryHeading->addWidget(m_count);
    m_totalTime = heading(QString(), "workspaceMuted", content);
    m_totalTime->setObjectName("workspaceTotalTime");
    m_totalTime->setProperty("pxlFlowHint", true);
    libraryHeading->addWidget(m_totalTime);
    libraryHeading->addStretch();
    m_viewModeButton = new QPushButton(content);
    m_viewModeButton->setObjectName("workspaceViewMode");
    m_viewModeButton->setFixedSize(40, 40);
    m_viewModeButton->setIconSize(QSize(20, 20));
    m_viewModeButton->setStyleSheet("QPushButton { background: rgba(53,54,60,185); border: 1px solid rgba(199,207,219,38); border-radius: 12px; padding: 8px; }"
                                  "QPushButton:hover, QPushButton:focus { background: rgba(77,65,57,220); border-color: rgba(242,163,101,130); }");
    libraryHeading->addWidget(m_viewModeButton);
    connect(m_viewModeButton, &QPushButton::clicked, this, [this] { setLibraryViewMode(!m_cardsMode); });
    contentLayout->addLayout(libraryHeading);
    auto* tools = new QHBoxLayout;
    tools->setSpacing(10);
    m_search = new Pxl::LibrarySearch(content);
    m_search->setObjectName("workspaceSearch");
    m_search->setPlaceholderText(tr("Search instances"));
    m_search->setAccessibleName(tr("Search instances"));
    tools->addWidget(m_search, 1);
    auto* importAction = new QPushButton(tr("Import"), content);
    importAction->setObjectName("workspaceHomeImport");
    importAction->setIcon(workspaceIcon("copy"));
    auto* create = new QPushButton(tr("New instance"), content);
    create->setObjectName("workspaceHomeCreate");
    create->setIcon(workspaceIcon("new"));
    auto* exportAction = new QPushButton(content);
    exportAction->setObjectName("workspaceHomeExport");
    exportAction->setIcon(workspaceIcon("export"));
    exportAction->setToolTip(tr("Share or transfer instances"));
    exportAction->setAccessibleName(tr("Share or transfer instances"));
    tools->addWidget(importAction);
    tools->addWidget(create);
    tools->addWidget(exportAction);
    tools->addStretch();
    contentLayout->addLayout(tools);
    m_libraryFilters = new QWidget(content);
    auto* filters = new QHBoxLayout(m_libraryFilters);
    filters->setContentsMargins(0, 0, 0, 0);
    m_groups = new Pxl::ChoiceChip(content);
    m_groups->setObjectName("workspaceGroups");
    m_groups->setAccessibleName(tr("Filter by group"));
    m_groups->setMinimumWidth(155);
    auto* sort = new Pxl::ChoiceChip(content);
    sort->setObjectName("workspaceSort");
    sort->setAccessibleName(tr("Sort instances"));
    sort->addItems({tr("Name"), tr("Last played")});
    filters->addWidget(m_groups);
    filters->addWidget(sort);
    filters->addStretch();
    filters->addWidget(heading(tr("Enter to open · Ctrl+Enter to play"), "workspaceMuted", content));
    contentLayout->addWidget(m_libraryFilters);
    m_libraryView = new QListView(content);
    m_libraryView->setObjectName("workspaceCards");
    m_libraryView->setViewMode(QListView::ListMode);
    m_libraryView->setResizeMode(QListView::Adjust);
    m_libraryView->setWrapping(false);
    m_libraryView->setMovement(QListView::Static);
    m_libraryView->setAccessibleName(tr("Instances. Enter opens settings, Ctrl+Enter plays."));
    m_libraryView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    contentLayout->addWidget(m_libraryView);
    m_emptyState = new QWidget(content);
    auto* empty = new QVBoxLayout(m_emptyState);
    empty->setContentsMargins(24, 24, 24, 24);
    empty->setSpacing(24);
    empty->addWidget(new WorkspaceBanner(m_emptyState));
    m_libraryEmpty = heading(tr("Your library starts here"), "workspaceEmptyTitle", m_emptyState);
    m_libraryEmpty->setAlignment(Qt::AlignCenter);
    empty->addWidget(m_libraryEmpty);
    auto* emptyDetail = heading(tr("Create an instance or import your Minecraft setup."), "workspaceMuted", m_emptyState);
    emptyDetail->setProperty("pxlSourceText", "Create an instance or import your Minecraft setup.");
    emptyDetail->setWordWrap(true);
    emptyDetail->setAlignment(Qt::AlignCenter);
    empty->addWidget(emptyDetail);
    auto* emptyActions = new QWidget(m_emptyState);
    emptyActions->setObjectName("workspaceEmptyActions");
    auto* emptyRow = new QHBoxLayout(emptyActions);
    emptyRow->setContentsMargins(0, 8, 0, 0);
    emptyRow->setSpacing(12);
    emptyRow->addStretch();
    auto* emptyCreate = new QPushButton(tr("Create instance"), emptyActions);
    emptyCreate->setObjectName("workspaceEmptyCreate");
    emptyCreate->setProperty("pxlPrimary", true);
    emptyCreate->setIcon(Pxl::primaryIcon("plus"));
    emptyCreate->setMinimumSize(200, 48);
    auto* emptyImport = new QPushButton(tr("Import"), emptyActions);
    emptyImport->setObjectName("workspaceEmptyImport");
    emptyImport->setIcon(workspaceIcon("copy"));
    emptyImport->setMinimumSize(160, 48);
    emptyRow->addWidget(emptyCreate);
    emptyRow->addWidget(emptyImport);
    emptyRow->addStretch();
    empty->addWidget(emptyActions);
    connect(emptyCreate, &QPushButton::clicked, this, &WorkspacePage::createRequested);
    connect(emptyImport, &QPushButton::clicked, this, &WorkspacePage::importRequested);
    contentLayout->addWidget(m_emptyState);
    contentLayout->addStretch();

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
        view->setItemDelegate(new WorkspaceCardDelegate(
            [this](const QModelIndex& index) { emit activateInstance(idForIndex(index)); },
            [this](const QModelIndex& index) { emit stopInstance(idForIndex(index)); },
            [this](const QModelIndex& index, const QPoint& point) { emit contextMenuRequested(idForIndex(index), point); }, view));
        connect(view, &QListView::activated, this, [this](const QModelIndex& index) {
            auto* instance = m_instances->getInstanceById(idForIndex(index));
            if (instance && instance->canEdit()) emit openInstanceRequested(instance->id());
        });
        connect(view->selectionModel(), &QItemSelectionModel::currentChanged, this,
                [this, view](const QModelIndex& current) { syncSelection(view, current); });
        connect(view, &QWidget::customContextMenuRequested, this, [this, view](const QPoint& point) { showContextMenu(view, point); });
    }
    m_libraryView->viewport()->installEventFilter(this);
    m_libraryView->installEventFilter(this);
    m_libraryView->viewport()->setAutoFillBackground(false);
    m_homeView->viewport()->setAutoFillBackground(false);
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
    connect(m_featuredPlay, &QPushButton::clicked, this, [this] {
        if (!m_featuredPlay->isEnabled() || m_featuredId.isEmpty()) return;
        auto* instance = m_instances ? m_instances->getInstanceById(m_featuredId) : nullptr;
        if (instance && instance->isRunning()) emit stopInstance(m_featuredId);
        else emit activateInstance(m_featuredId);
    });
    connect(manageFeatured, &QPushButton::clicked, this, [this] {
        if (!m_featuredId.isEmpty()) emit openInstanceRequested(m_featuredId);
    });
    connect(sort, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        static_cast<WorkspaceInstanceModel*>(m_libraryModel)->setRecentOrder(index == 1);
        m_libraryModel->sort(0);
    });
    auto filtered = [this] { updateLayout(); setSelectedInstance(m_selectedId); };
    connect(m_search, &QLineEdit::textChanged, this, [this, filtered](const QString& text) {
        m_syncingSelection = true;
        static_cast<WorkspaceInstanceModel*>(m_libraryModel)->setSearchText(text);
        m_syncingSelection = false;
        filtered();
    });
    connect(m_groups, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, filtered](int) {
        m_syncingSelection = true;
        static_cast<WorkspaceInstanceModel*>(m_libraryModel)->setGroup(m_groups->currentData().toString());
        m_syncingSelection = false;
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
    setLibraryViewMode(m_preferences && m_preferences->get("WorkspaceLibraryView").toString() != "list");
    showHome(true);
    setNavigationPosition(false);
    retranslateUi();
    auto* playtimeClock = new QTimer(this);
    playtimeClock->setInterval(30000);
    connect(playtimeClock, &QTimer::timeout, this, [this] { updateHome(); m_libraryView->viewport()->update(); });
    playtimeClock->start();
    if (auto* app = APPLICATION_DYN) {
        auto applyBackground = [this, app] {
            setImageBackground(app->settings()->get("WorkspaceBackground").toString() != "solid",
                               app->settings()->get("WorkspaceBackgroundImage").toString());
            setNavigationPosition(app->settings()->get("WorkspaceNavigation").toString() == "top");
            setLibraryViewMode(app->settings()->get("WorkspaceLibraryView").toString() != "list");
        };
        connect(app, &Application::globalSettingsApplied, this, applyBackground);
        applyBackground();
    } else if (m_preferences) setLibraryViewMode(m_preferences->get("WorkspaceLibraryView").toString() != "list");
}

WorkspacePage::~WorkspacePage()
{
    for (auto* instance : m_observedInstances) disconnect(instance, nullptr, this, nullptr);
    // QWidget tears down its children after our members. A child view's
    // destroyed callback must not access the already-destroyed page state.
    for (auto* view : m_contentStack->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly))
        disconnect(view, nullptr, this, nullptr);
}

void WorkspacePage::setLibraryViewMode(bool cards)
{
    m_cardsMode = cards;
    m_libraryView->setProperty("pxlCards", cards);
    m_libraryView->setViewMode(cards ? QListView::IconMode : QListView::ListMode);
    m_libraryView->setFlow(cards ? QListView::LeftToRight : QListView::TopToBottom);
    m_libraryView->setWrapping(cards);
    m_libraryView->setMovement(QListView::Static);
    m_libraryView->setSpacing(cards ? 8 : 0);
    if (!cards) m_libraryView->setGridSize(QSize());
    m_viewModeButton->setIcon(QIcon(cards ? ":/pxl/icons/list.svg" : ":/pxl/icons/squares-four.svg"));
    const auto action = cards ? tr("Switch to list") : tr("Switch to cards");
    m_viewModeButton->setToolTip(action);
    m_viewModeButton->setAccessibleName(action);
    if (m_preferences) m_preferences->set("WorkspaceLibraryView", cards ? "cards" : "list");
    updateLayout();
    m_libraryView->doItemsLayout();
    m_libraryView->viewport()->update();
}

void WorkspacePage::showHome(bool home)
{
    if (!leaveInstanceView()) {
        updateActiveNavigation();
        return;
    }
    const bool changed = m_showHome != home;
    m_showHome = home;
    m_heading->setText(home ? tr("Home") : tr("Library"));
    m_header->setVisible(true);
    m_homeButton->setChecked(home);
    m_libraryButton->setChecked(!home);
    m_syncingSelection = true;
    auto* library = static_cast<WorkspaceInstanceModel*>(m_libraryModel);
    library->setRecentOrder(home || findChild<QComboBox*>("workspaceSort")->currentIndex() == 1);
    library->setSearchText(m_search->text());
    library->setGroup(m_groups->currentData().toString());
    updateHome();
    m_syncingSelection = false;
    setSelectedInstance(m_selectedId);
    m_scroll->verticalScrollBar()->setValue(0);
    if (changed) Pxl::animatePage(m_scroll);
}

void WorkspacePage::updateLayout()
{
    const int count = m_libraryModel->rowCount();
    const int total = (m_instances ? m_instances->rowCount() : 0) - (m_showHome && !m_featuredId.isEmpty() ? 1 : 0);
    const bool emptyHome = m_showHome && (!m_instances || m_instances->rowCount() == 0);
    findChild<QWidget*>("workspaceEmptyActions")->setVisible(emptyHome);
    for (const auto* name : {"workspaceHomeCreate", "workspaceHomeImport", "workspaceHomeExport"})
        findChild<QPushButton*>(name)->setVisible(!emptyHome);
    m_count->setText(m_showHome || count == total ? (count == 1 ? tr("1 instance") : tr("%1 instances").arg(count))
                                  : tr("%1 of %2").arg(count).arg(total));
    m_recentSection->setVisible(m_showHome && !m_featuredId.isEmpty());
    m_count->setVisible(!m_showHome || total > 0);
    m_libraryTitle->setVisible(m_showHome && total > 0);
    m_libraryTitle->setText(tr("Library"));
    const bool hasInstances = m_instances && m_instances->rowCount() > 0;
    m_search->setVisible(!m_showHome || total > 0);
    m_libraryFilters->setVisible(!m_showHome || total > 0);
    m_viewModeButton->setVisible(!m_showHome || total > 0);
    m_emptyState->setVisible(count == 0 && (!m_showHome || total > 0 || m_featuredId.isEmpty()));
    m_libraryEmpty->setText(total == 0 ? (m_featuredId.isEmpty() ? tr("Your library starts here") : tr("Add another instance"))
                                     : tr("No matching instances"));
    m_libraryView->setVisible(count > 0);
    if (m_cardsMode) {
        const int available = qMax(220, m_libraryView->viewport()->width());
        const int columns = qMax(1, available / 236);
        const int tileWidth = qMax(210, available / columns - 16);
        m_libraryView->setGridSize(QSize(tileWidth, 260));
        const int rows = (count + columns - 1) / columns;
        m_libraryView->setFixedHeight(qMax(1, qMin(rows, 4)) * 276 + 8);
    } else m_libraryView->setFixedHeight(qMax(1, qMin(count, 9)) * 76 + 4);
    qint64 seconds = 0;
    bool showTotal = false;
    if (m_preferences) showTotal = m_preferences->get("ShowGlobalGameTime").toBool();
    if (m_instances) for (int i = 0; i < m_instances->rowCount(); ++i) {
        auto* instance = static_cast<BaseInstance*>(m_instances->index(i, 0).data(InstanceList::InstancePointerRole).value<void*>());
        if (!instance || instance->currentStatus() != BaseInstance::Status::Present) continue;
        seconds += qMax<qint64>(0, instance->totalTimePlayed());
    }
    m_totalTime->setVisible(showTotal && hasInstances);
    m_totalTime->setText(tr("%1 h %2 min in total").arg(seconds / 3600).arg((seconds / 60) % 60));
    m_catPreview->setVisible(m_contentStack->currentWidget() == m_scroll && m_showHome && m_catPreview->property("pxlCatEnabled").toBool());
    m_catPreview->move(qMax(0, width() - 108), qMax(0, height() - 116));
    m_catPreview->raise();
}

void WorkspacePage::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (property("pxlTopNavigation").toBool()) setNavigationPosition(true);
    QTimer::singleShot(0, this, &WorkspacePage::updateLayout);
}

bool WorkspacePage::eventFilter(QObject* object, QEvent* event)
{
    if (object == m_libraryView->viewport() && event->type() == QEvent::Resize)
        QTimer::singleShot(0, this, &WorkspacePage::updateLayout);
    if (object == m_libraryView->viewport() && m_cardsMode) {
        if (event->type() == QEvent::MouseMove) {
            const auto* mouse = static_cast<QMouseEvent*>(event);
            const auto index = m_libraryView->indexAt(mouse->position().toPoint());
            if (index.isValid()) m_libraryView->viewport()->update(m_libraryView->visualRect(index));
        }
        if (event->type() == QEvent::MouseButtonDblClick) {
            m_libraryView->setProperty("pxlIgnoreDoubleRelease", true);
            return true;
        }
        if (event->type() == QEvent::MouseButtonRelease) {
            const auto* mouse = static_cast<QMouseEvent*>(event);
            if (mouse->button() == Qt::LeftButton) {
                if (m_libraryView->property("pxlIgnoreDoubleRelease").toBool()) {
                    m_libraryView->setProperty("pxlIgnoreDoubleRelease", false);
                    return true;
                }
                const auto index = m_libraryView->indexAt(mouse->position().toPoint());
                auto* delegate = static_cast<WorkspaceCardDelegate*>(m_libraryView->itemDelegate());
                const auto row = m_libraryView->visualRect(index).adjusted(0, 2, -4, -2);
                if (index.isValid() && !delegate->playRect(row).contains(mouse->position().toPoint())
                    && !delegate->moreRect(row).contains(mouse->position().toPoint())) {
                    const auto id = idForIndex(index);
                    auto* instance = m_instances ? m_instances->getInstanceById(id) : nullptr;
                    if (instance && instance->canEdit()) emit openInstanceRequested(id);
                    return true;
                }
            }
        }
    }
    if (object == m_libraryView && event->type() == QEvent::KeyPress) {
        const auto* key = static_cast<QKeyEvent*>(event);
        if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) && key->modifiers().testFlag(Qt::ControlModifier)) {
            const auto id = idForIndex(m_libraryView->currentIndex());
            auto* instance = m_instances ? m_instances->getInstanceById(id) : nullptr;
            if (instance && instance->canLaunch() && !instance->isRunning()) emit activateInstance(id);
            return true;
        }
    }
    return QWidget::eventFilter(object, event);
}

void WorkspacePage::setSelectedInstance(const QString& id)
{
    m_selectedId = id;
    m_syncingSelection = true;
    selectInView(m_homeView, id);
    selectInView(m_libraryView, id);
    if (m_showHome && m_recentModel->rowCount() == 0 && m_featuredId != id) updateHome();
    m_syncingSelection = false;
}

void WorkspacePage::refresh()
{
    if (m_instances) for (int i = 0; i < m_instances->rowCount(); ++i) {
        auto* instance = static_cast<BaseInstance*>(m_instances->index(i, 0).data(InstanceList::InstancePointerRole).value<void*>());
        if (!instance || m_observedInstances.contains(instance)) continue;
        m_observedInstances.insert(instance);
        connect(instance, &BaseInstance::runningStatusChanged, this, [this] { refresh(); });
        connect(instance, &QObject::destroyed, this, [this, instance] { m_observedInstances.remove(instance); });
    }
    m_syncingSelection = true;
    updateGroups();
    static_cast<WorkspaceInstanceModel*>(m_recentModel)->rebuildMetadata();
    static_cast<WorkspaceInstanceModel*>(m_libraryModel)->rebuildMetadata();
    m_recentModel->sort(0);
    m_libraryModel->sort(0);
    updateHome();
    updateLayout();
    m_syncingSelection = false;
    setSelectedInstance(m_selectedId);
    m_libraryView->viewport()->update();
    m_homeView->viewport()->update();
}

void WorkspacePage::setPaintCat(bool visible)
{
    m_catPreview->setPaintCat(visible);
    m_catPreview->setProperty("pxlCatEnabled", visible);
    m_catPreview->setVisible(visible && m_showHome && m_contentStack->currentWidget() == m_scroll);
}
void WorkspacePage::setCatButtonVisible(bool visible) { m_catButton->setVisible(visible); }

void WorkspacePage::setNavigationPosition(bool top)
{
    setProperty("pxlTopNavigation", top);
    m_rootLayout->setDirection(top ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
    m_navigationLayout->setDirection(top ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);
    m_navigation->setMinimumSize(0, 0);
    m_navigation->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    const bool compactTop = top && width() < 960;
    m_navigation->findChild<QLabel*>("workspaceBrandName")->setVisible(top && !compactTop);
    m_navigation->findChild<QLabel*>("workspaceBrandIcon")->setFixedSize(top ? 40 : 48, 40);
    m_rootLayout->setContentsMargins(top ? 0 : 8, top ? 0 : 8, 0, top ? 0 : 8);
    m_navigation->setStyleSheet(top ? QString() : QStringLiteral(
        "QFrame#workspaceRail { background: rgba(32, 35, 40, 125); border: 1px solid rgba(164, 173, 186, 24); border-radius: 14px; }"));
    m_navigationLayout->setContentsMargins(top ? 16 : 10, 16, top ? 16 : 10, 16);
    m_navigationLayout->setSpacing(top ? 10 : 8);
    if (top) {
        m_navigation->setFixedHeight(72);
        m_navigation->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_catPreview->setFixedSize(80, 88);
    } else {
        m_navigation->setFixedWidth(68);
        m_navigation->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        m_catPreview->setFixedSize(80, 88);
    }
    for (int i = 0; i < m_navigationButtons.size(); ++i) {
        auto* button = m_navigationButtons[i];
        const bool iconOnly = !top || compactTop || i >= 3;
        button->setText(iconOnly ? QString() : button->property("pxlNavLabel").toString());
        button->setMinimumWidth(top ? (iconOnly ? 40 : 120) : 48);
        button->setMaximumWidth(top ? (iconOnly ? 40 : 180) : 48);
        button->setFixedHeight(top ? 42 : 48);
        button->setIconSize(QSize(22, 22));
        button->style()->unpolish(button);
        button->style()->polish(button);
        button->update();
    }
    QTimer::singleShot(0, this, &WorkspacePage::updateLayout);
}

void WorkspacePage::setImageBackground(bool enabled, const QString& path)
{
    if (enabled) {
        m_backgroundImage = Pxl::blurredBackdrop(path);
    }
    m_imageBackground = enabled && !m_backgroundImage.isNull();
    static_cast<WorkspaceHero*>(m_recentSection)->setArtworkEnabled(m_imageBackground);
    update();
}

void WorkspacePage::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.fillRect(rect(), palette().color(QPalette::Window));
    if (!m_imageBackground) return;
    Pxl::paintBackdrop(painter, rect(), m_backgroundImage, 148);
}
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

void WorkspacePage::updateHome()
{
    auto featured = m_recentModel->rowCount() > 0 ? m_recentModel->index(0, 0) : QModelIndex();
    // A new, never-played library still has a useful launch dashboard.
    if (!featured.isValid() && m_instances) {
        for (int row = 0; row < m_instances->rowCount(); ++row) {
            const auto source = m_instances->index(row, 0);
            auto* candidate = static_cast<BaseInstance*>(source.data(InstanceList::InstancePointerRole).value<void*>());
            if (!candidate || candidate->currentStatus() != BaseInstance::Status::Present) continue;
            if (!featured.isValid()) featured = source;
            if (candidate->id() == m_selectedId) {
                featured = source;
                break;
            }
        }
    }
    m_featuredId = idForIndex(featured);
    auto* instance = m_instances ? m_instances->getInstanceById(m_featuredId) : nullptr;
    m_featuredName->setText(featured.data(Qt::DisplayRole).toString());
    auto metadata = featured.data(kWorkspaceMetadataRole).toString();
    if (metadata.isEmpty() && instance) metadata = tr("Minecraft");
    m_featuredDetail->setText(metadata);
    const auto duration = playedDuration(instance);
    m_featuredTime->setText(lastPlayedText(instance) + (duration.isEmpty() ? QString() : " · " + duration));
    const bool running = instance && instance->isRunning();
    m_featuredPlay->setText(running ? tr("Stop") : tr("Play"));
    m_featuredPlay->setProperty("pxlPrimary", !running);
    m_featuredPlay->setProperty("pxlStop", running);
    m_featuredPlay->setAccessibleName(running ? tr("Stop instance") : tr("Play instance"));
    m_featuredPlay->style()->unpolish(m_featuredPlay);
    m_featuredPlay->style()->polish(m_featuredPlay);
    auto playPixmap = QIcon(running ? ":/pxl/icons/stop.svg" : ":/pxl/icons/play.svg").pixmap(20, 20);
    {
        QPainter painter(&playPixmap);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(playPixmap.rect(), running ? QColor("#ffe8e8") : palette().color(QPalette::HighlightedText));
    }
    m_featuredPlay->setIcon(QIcon(playPixmap));
    m_featuredPlay->setEnabled(m_showHome && instance && (instance->canLaunch() || running));
    static_cast<WorkspaceInstanceModel*>(m_libraryModel)->setExcludedId(m_showHome ? m_featuredId : QString());
    updateLayout();
}

void WorkspacePage::showContextMenu(QListView* view, const QPoint& position)
{
    const auto id = idForIndex(view->indexAt(position));
    if (!id.isEmpty()) emit contextMenuRequested(id, view->viewport()->mapToGlobal(position));
}

void WorkspacePage::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LanguageChange && m_libraryModel) retranslateUi();
}

void WorkspacePage::retranslateUi()
{
    const char* labels[] = {"Home", "Library", "Import", "Downloads & tasks", "Cat", "Advanced", "Accounts", "Settings"};
    for (int i = 0; i < m_navigationButtons.size(); ++i) {
        auto* button = m_navigationButtons[i];
        const auto text = tr(labels[i]);
        button->setProperty("pxlNavLabel", text);
        button->setToolTip(text);
        button->setAccessibleName(text);
    }
    const QHash<QString, const char*> buttons{{"workspaceHomeCreate", "New instance"}, {"workspaceHomeImport", "Import"},
        {"workspaceEmptyCreate", "Create instance"}, {"workspaceEmptyImport", "Import"},
        {"workspaceConfigureFeatured", "Configure"}, {"workspaceBrowseMods", "Find content"}};
    for (auto it = buttons.cbegin(); it != buttons.cend(); ++it)
        if (auto* button = findChild<QPushButton*>(it.key())) button->setText(tr(it.value()));
    for (auto* label : findChildren<QLabel*>()) {
        const auto source = label->property("pxlSourceText").toByteArray();
        if (!source.isEmpty()) label->setText(tr(source.constData()));
    }
    m_search->setPlaceholderText(tr("Search instances"));
    m_search->setAccessibleName(tr("Search instances"));
    m_groups->setAccessibleName(tr("Filter by group"));
    auto* sort = findChild<QComboBox*>("workspaceSort");
    sort->setItemText(0, tr("Name"));
    sort->setItemText(1, tr("Last played"));
    for (auto* label : findChildren<QLabel*>("workspaceMuted"))
        if (label->text().contains("Ctrl+Enter")) label->setText(tr("Enter to open · Ctrl+Enter to play"));
    m_heading->setText(m_contentStack->currentWidget() == m_scroll ? (m_showHome ? tr("Home") : tr("Library")) : m_instanceViewTitle);
    m_backButton->setText(tr("Back"));
    m_viewModeButton->setToolTip(m_cardsMode ? tr("Switch to list") : tr("Switch to cards"));
    m_viewModeButton->setAccessibleName(m_viewModeButton->toolTip());
    updateGroups();
    static_cast<WorkspaceInstanceModel*>(m_recentModel)->rebuildMetadata();
    static_cast<WorkspaceInstanceModel*>(m_libraryModel)->rebuildMetadata();
    updateHome();
    setNavigationPosition(property("pxlTopNavigation").toBool());
}

bool WorkspacePage::presentInstanceView(QWidget* view, const QString& title, std::function<bool()> leaveGuard)
{
    if (!view) return false;
    if (m_instanceView && m_instanceView != view) {
        if (!leaveInstanceView()) return false;
        m_contentStack->removeWidget(m_instanceView);
        m_instanceView->deleteLater();
    }
    if (m_instanceView != view) {
        m_contentStack->addWidget(view);
        connect(view, &QObject::destroyed, this, [this, view] {
            if (!m_instanceView || m_instanceView.data() == view) {
                m_instanceView.clear();
                m_leaveInstanceGuard = {};
                m_backButton->hide();
                m_heading->setText(m_showHome ? tr("Home") : tr("Library"));
                QTimer::singleShot(0, this, [this] {
                    if (!m_instanceView) {
                        m_contentStack->setCurrentWidget(m_scroll);
                        updateActiveNavigation();
                        updateLayout();
                    }
                });
            }
        });
    }
    m_instanceView = view;
    m_instanceViewTitle = title;
    m_leaveInstanceGuard = std::move(leaveGuard);
    m_contentStack->setCurrentWidget(view);
    updateActiveNavigation();
    m_heading->setText(title);
    m_backButton->show();
    m_catPreview->hide();
    Pxl::animatePage(view);
    return true;
}

bool WorkspacePage::leaveInstanceView()
{
    if (m_contentStack->currentWidget() == m_scroll) return true;
    if (m_leaveInstanceGuard && !m_leaveInstanceGuard()) return false;
    m_contentStack->setCurrentWidget(m_scroll);
    updateActiveNavigation();
    m_backButton->hide();
    m_heading->setText(m_showHome ? tr("Home") : tr("Library"));
    updateLayout();
    Pxl::animatePage(m_scroll);
    return true;
}

void WorkspacePage::updateActiveNavigation()
{
    const auto* current = m_contentStack->currentWidget();
    QString active = current ? current->property("pxlNavigation").toString() : QString();
    if (active.isEmpty()) active = m_showHome ? "workspaceHome" : "workspaceLibrary";
    for (auto* button : m_navigationButtons) {
        if (button->isCheckable()) button->setChecked(button->objectName() == active);
    }
}

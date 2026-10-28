// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "Icons.h"
#include "InstanceCovers.h"
#include "InstanceDescription.h"
#include <BaseInstance.h>
#include <InstanceList.h>
#include <QApplication>
#include <QCursor>
#include <QListView>
#include <QMouseEvent>
#include <QPainterPath>
#include <QStyledItemDelegate>
#include <functional>

namespace Pxl {
// All presentations share the same source data and hit targets as their drawing.
class InstanceLibraryDelegate final : public QStyledItemDelegate {
public:
    using Activate = std::function<void(const QModelIndex&)>;
    using Context = std::function<void(const QModelIndex&, const QPoint&)>;
    using Format = std::function<QString(BaseInstance*)>;
    InstanceLibraryDelegate(Activate activate, Activate stop, Context context, int metadataRole,
                            Format lastPlayed, Format duration, QObject* parent)
        : QStyledItemDelegate(parent), m_activate(std::move(activate)), m_stop(std::move(stop)),
          m_context(std::move(context)), m_metadataRole(metadataRole), m_lastPlayed(std::move(lastPlayed)), m_duration(std::move(duration)) {}
    QString mode() const { return parent()->property("pxlLibraryMode").toString(); }
    bool cards() const { return mode() == "covers" || mode() == "hybrid"; }
    bool hybrid() const { return mode() == "hybrid"; }
    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override
    {
        const auto* view = qobject_cast<QListView*>(parent());
        return cards() ? QSize(view && view->gridSize().width() > 0 ? view->gridSize().width() : 300, hybrid() ? 140 : 220)
                       : QSize(400, 74);
    }
    QRect playRect(const QRect& row) const
    {
        return QRect(row.right() - (cards() ? 104 : 136), cards() ? row.bottom() - 48 : row.center().y() - 17, 88, 34);
    }
    QRect moreRect(const QRect& row) const
    {
        return QRect(row.right() - 40, cards() ? row.top() + 8 : row.center().y() - 14, 28, 28);
    }
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setRenderHint(QPainter::SmoothPixmapTransform);
        const QRect row = option.rect.adjusted(0, 2, -4, -2);
        const bool selected = option.state.testFlag(QStyle::State_Selected);
        const bool hover = option.state.testFlag(QStyle::State_MouseOver);
        painter->setPen(selected ? QColor(242,163,101,115) : QColor(185,194,206,30));
        painter->setBrush(hover ? QColor(44,46,51,210) : QColor(30,33,38,183));
        painter->drawRoundedRect(row.adjusted(1,1,-1,-1), 16, 16);
        auto* instance = static_cast<BaseInstance*>(index.data(InstanceList::InstancePointerRole).value<void*>());
        const auto icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        const auto group = index.data(InstanceList::GroupRole).toString();
        auto metadata = index.data(m_metadataRole).toString();
        if (!group.isEmpty()) metadata += " · " + group;
        const auto state = instance && instance->isRunning() ? tr("Running") : m_lastPlayed(instance);
        const auto duration = m_duration(instance);
        auto text = [&](QRect area, const QString& value, int size, bool bold = false, bool muted = false) {
            QFont font = option.font;
            font.setPixelSize(size);
            font.setWeight(bold ? QFont::DemiBold : QFont::Medium);
            painter->setFont(font);
            painter->setPen(muted ? QColor("#adb7c6") : QColor("#f0f1f3"));
            painter->drawText(area, Qt::AlignVCenter, QFontMetrics(font).elidedText(value, Qt::ElideRight, qMax(0,area.width())));
        };
        if (mode() == "covers") {
            const QRect artwork(row.left()+1, row.top()+1, row.width()-2, 78);
            painter->save();
            QPainterPath clip; clip.addRoundedRect(row.adjusted(1,1,-1,-1), 15,15); painter->setClipPath(clip);
            QLinearGradient neutral(artwork.topLeft(), artwork.bottomRight());
            neutral.setColorAt(0,QColor("#343b40")); neutral.setColorAt(0.6,QColor("#2e3239")); neutral.setColorAt(1,QColor("#49413d"));
            painter->fillRect(artwork,neutral);
            const auto cover = instance ? instanceCover(instance->instanceRoot()) : QPixmap();
            if (!cover.isNull()) {
                const auto crop = cover.scaled(artwork.size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
                painter->drawPixmap(artwork, crop, QRect((crop.width()-artwork.width())/2,(crop.height()-artwork.height())/2,artwork.width(),artwork.height()));
                painter->fillRect(artwork,QColor(16,20,25,35));
            }
            painter->restore();
            icon.paint(painter,QRect(row.left()+16,row.top()+15,50,50));
            text(QRect(row.left()+16,row.top()+89,row.width()-32,24),index.data().toString(),16,true);
            text(QRect(row.left()+16,row.top()+116,row.width()-32,20),metadata,12,false,true);
            text(QRect(row.left()+16,row.top()+143,row.width()-32,18),state,12,false,true);
            text(QRect(row.left()+16,playRect(row).top(),qMax(0,row.width()-128),34),duration,12,false,true);
        } else if (hybrid()) {
            icon.paint(painter,QRect(row.left()+16,row.top()+16,52,52));
            text(QRect(row.left()+82,row.top()+13,row.width()-130,25),index.data().toString(),16,true);
            text(QRect(row.left()+82,row.top()+40,row.width()-100,22),metadata,12,false,true);
            text(QRect(row.left()+82,row.top()+64,row.width()-100,18),state,12,false,true);
            text(QRect(row.left()+16,playRect(row).top(),qMax(0,row.width()-128),34),duration,12,false,true);
        } else {
            icon.paint(painter,QRect(row.left()+14,row.center().y()-22,44,44));
            const bool wide = row.width() >= 760;
            const int identityWidth = qMax(40,row.width() - (wide ? 390 : 224));
            const auto description = instance ? briefInstanceDescription(instance->description()) : QString();
            if (!description.isEmpty() && identityWidth >= 220) {
                text(QRect(row.left()+72,row.top()+5,identityWidth,22),index.data().toString(),15,true);
                text(QRect(row.left()+72,row.top()+27,identityWidth,18),description,12,false,true);
                text(QRect(row.left()+72,row.top()+46,identityWidth,18),metadata,11,false,true);
            } else {
                text(QRect(row.left()+72,row.top()+11,identityWidth,24),index.data().toString(),15,true);
                text(QRect(row.left()+72,row.top()+38,identityWidth,20),metadata,12,false,true);
            }
            if (wide) {
                text(QRect(row.right()-302,row.top()+12,150,22),state,12,false,true);
                text(QRect(row.right()-302,row.top()+38,150,20),duration,12,false,true);
            }
        }
        const auto action = playRect(row);
        const auto* view = qobject_cast<QListView*>(parent());
        const bool overPlay = hover && view && action.contains(view->viewport()->mapFromGlobal(QCursor::pos()));
        const bool running = instance && instance->isRunning();
        const bool enabled = instance && (instance->canLaunch() || running);
        const bool bright = enabled && (overPlay || option.state.testFlag(QStyle::State_HasFocus));
        painter->setPen(QPen(running ? QColor(205,102,108,100) : QColor(242,163,101,enabled ? 100 : 25),1));
        painter->setBrush(running ? QColor(bright ? "#a34d54" : "#633c42")
                                 : bright ? QColor("#f2a365") : QColor(242,163,101,enabled ? 38 : 12));
        painter->drawRoundedRect(action,10,10);
        const auto glyph = running ? QIcon(":/pxl/icons/stop.svg") : bright ? primaryIcon("play") : QIcon(":/pxl/icons/play.svg");
        glyph.paint(painter,QRect(action.left()+11,action.top()+9,16,16),Qt::AlignCenter,enabled ? QIcon::Normal : QIcon::Disabled);
        QFont button = option.font; button.setPixelSize(12); button.setWeight(QFont::DemiBold); painter->setFont(button);
        painter->setPen(!enabled ? QColor("#7d858e") : bright && !running ? QColor("#251c16") : QColor("#f4ded0"));
        painter->drawText(action.adjusted(31,0,-5,0),Qt::AlignVCenter,running ? tr("Stop") : tr("Play"));
        const auto more = moreRect(row);
        if (mode() == "covers") { painter->setPen(Qt::NoPen); painter->setBrush(QColor(20,24,29,160)); painter->drawRoundedRect(more,8,8); }
        QIcon(":/pxl/icons/dots-three-vertical.svg").paint(painter,more.adjusted(5,5,-5,-5));
        painter->restore();
    }
    bool editorEvent(QEvent* event, QAbstractItemModel*, const QStyleOptionViewItem& option, const QModelIndex& index) override
    {
        if (event->type() != QEvent::MouseButtonRelease) return false;
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() != Qt::LeftButton) return false;
        const auto row = option.rect.adjusted(0,2,-4,-2);
        if (moreRect(row).contains(mouse->position().toPoint())) { m_context(index,mouse->globalPosition().toPoint()); return true; }
        if (playRect(row).contains(mouse->position().toPoint())) {
            auto* instance = static_cast<BaseInstance*>(index.data(InstanceList::InstancePointerRole).value<void*>());
            if (instance && instance->isRunning()) m_stop(index);
            else if (instance && instance->canLaunch()) m_activate(index);
            return true;
        }
        return false;
    }
private:
    Activate m_activate, m_stop;
    Context m_context;
    int m_metadataRole;
    Format m_lastPlayed, m_duration;
};
} // namespace Pxl

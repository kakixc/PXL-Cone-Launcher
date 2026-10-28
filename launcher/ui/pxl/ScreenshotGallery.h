// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "Backdrop.h"
#include "DialogOverlay.h"
#include "FrostedSurface.h"
#include "InstanceCovers.h"
#include "LibraryControls.h"
#include "MessagePanel.h"
#include <InstanceList.h>
#include <minecraft/MinecraftInstance.h>
#include <QAbstractListModel>
#include <QApplication>
#include <QClipboard>
#include <QImageReader>
#include <algorithm>
#include <QCache>
#include <QDesktopServices>
#include <QDirIterator>
#include <QFileDialog>
#include <QFutureWatcher>
#include <QListView>
#include <QKeyEvent>
#include <QCloseEvent>
#include <QCursor>
#include <QVariantAnimation>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QRegularExpression>
#include <QtConcurrent/QtConcurrentRun>

namespace Pxl {
struct ScreenshotEntry {
    QString file, instanceId, instanceName, instanceRoot, folder;
    qint64 modified = 0;
};
inline QString screenshotText(const char* text) { return QCoreApplication::translate("Pxl::ScreenshotGallery",text); }
inline bool copyScreenshotImage(const QString& path)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const auto size = reader.size();
    if (!size.isValid() || size.width() > 16384 || size.height() > 16384 || qint64(size.width()) * size.height() > 64000000) return false;
    const auto image = reader.read();
    if (image.isNull()) return false;
    QApplication::clipboard()->setImage(image);
    return true;
}
inline QList<ScreenshotEntry> scanScreenshots(const QList<ScreenshotEntry>& roots)
{
    QList<ScreenshotEntry> result;
    for (const auto& root : roots) {
        const QFileInfo folder(root.folder);
        const auto boundary=QFileInfo(root.instanceRoot).canonicalFilePath();
        const auto canonical=folder.canonicalFilePath();
        const auto relative=QDir(boundary).relativeFilePath(canonical);
        if (folder.isSymLink() || !folder.isDir() || boundary.isEmpty() || canonical.isEmpty() ||
            relative=="." || relative==".." || relative.startsWith("../") || QDir::isAbsolutePath(relative)) continue;
        QDirIterator files(root.folder,{"*.png","*.jpg","*.jpeg","*.webp"},QDir::Files | QDir::NoSymLinks,QDirIterator::Subdirectories);
        while (files.hasNext()) {
            ScreenshotEntry entry=root;
            entry.file=files.next(); entry.modified=files.fileInfo().lastModified().toMSecsSinceEpoch();
            result.append(entry);
        }
    }
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b) { return a.modified>b.modified; });
    return result;
}
class ScreenshotModel final : public QAbstractListModel {
public:
    enum { InstanceRole=Qt::UserRole+1 };
    explicit ScreenshotModel(QObject* parent) : QAbstractListModel(parent),m_cache(16*1024) {}
    int rowCount(const QModelIndex& parent={}) const override { return parent.isValid() ? 0 : m_entries.size(); }
    ScreenshotEntry entry(int row) const { return m_entries.value(row); }
    void replace(QList<ScreenshotEntry> entries) { beginResetModel(); m_entries=std::move(entries); endResetModel(); }
    QVariant data(const QModelIndex& index,int role) const override
    {
        if (!index.isValid() || index.row()>=m_entries.size()) return {};
        const auto item=entry(index.row());
        if (role==Qt::DisplayRole) return QFileInfo(item.file).completeBaseName();
        if (role==Qt::ToolTipRole || role==Qt::AccessibleTextRole) return item.instanceName+" — "+QFileInfo(item.file).fileName();
        if (role==InstanceRole) return item.instanceId;
        if (role==Qt::UserRole) return item.instanceName;
        if (role==Qt::DecorationRole) {
            const auto key=item.file+QString::number(item.modified);
            if (auto* cached=m_cache.object(key)) return *cached;
            const_cast<ScreenshotModel*>(this)->requestThumbnail(item,key);
            return QIcon(":/pxl/icons/image.svg").pixmap(48,48);
        }
        return {};
    }
private:
    void requestThumbnail(const ScreenshotEntry& entry,const QString& key)
    {
        if (m_pending.contains(key) || m_pending.size()>=4 || m_failed.contains(key)) return;
        m_pending.insert(key);
        auto* watcher=new QFutureWatcher<QImage>(this);
        connect(watcher,&QFutureWatcher<QImage>::finished,this,[this,watcher,key] {
            const auto image=watcher->result(); watcher->deleteLater(); m_pending.remove(key);
            if (image.isNull()) m_failed.insert(key);
            else { auto pixmap=QPixmap::fromImage(image); m_cache.insert(key,new QPixmap(pixmap),qMax(1,pixmap.width()*pixmap.height()*4/1024)); }
            if (rowCount()) emit dataChanged(index(0,0),index(rowCount()-1,0),{Qt::DecorationRole});
        });
        watcher->setFuture(QtConcurrent::run([path=entry.file] {
            return loadBackdropImage(path).scaled(320,180,Qt::KeepAspectRatio,Qt::SmoothTransformation);
        }));
    }
    QList<ScreenshotEntry> m_entries;
    mutable QCache<QString,QPixmap> m_cache;
    QSet<QString> m_pending,m_failed;
};
class ScreenshotDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem&,const QModelIndex&) const override { return {244,190}; }
    void paint(QPainter* p,const QStyleOptionViewItem& option,const QModelIndex& index) const override
    {
        p->save(); p->setRenderHint(QPainter::Antialiasing); p->setRenderHint(QPainter::SmoothPixmapTransform);
        const auto area=option.rect.adjusted(4,4,-4,-4);
        p->setPen(option.state.testFlag(QStyle::State_Selected) ? QColor("#f2a365") : QColor(186,198,214,36));
        p->setBrush(QColor(30,34,40,205)); p->drawRoundedRect(area,13,13);
        const auto image=qvariant_cast<QPixmap>(index.data(Qt::DecorationRole));
        const auto photo=area.adjusted(7,7,-7,-55);
        QPainterPath clip; clip.addRoundedRect(photo,9,9); p->setClipPath(clip);
        p->fillRect(photo,QColor("#30373d"));
        if (!image.isNull()) {
            const auto fit=image.scaled(photo.size(),Qt::KeepAspectRatioByExpanding,Qt::SmoothTransformation);
            p->drawPixmap(photo,fit,QRect((fit.width()-photo.width())/2,(fit.height()-photo.height())/2,photo.width(),photo.height()));
        }
        p->setClipping(false);
        auto font=option.font; font.setPixelSize(13); font.setWeight(QFont::DemiBold); p->setFont(font); p->setPen(QColor("#edf0f4"));
        p->drawText(area.adjusted(12,area.height()-49,-12,-24),Qt::AlignVCenter,QFontMetrics(font).elidedText(index.data().toString(),Qt::ElideRight,area.width()-24));
        font.setPixelSize(12); font.setWeight(QFont::Medium); p->setFont(font); p->setPen(QColor("#aab5c4"));
        p->drawText(area.adjusted(12,area.height()-27,-12,-5),Qt::AlignVCenter,QFontMetrics(font).elidedText(index.data(Qt::UserRole).toString(),Qt::ElideRight,area.width()-24));
        p->restore();
    }
};
inline QStringList exportScreenshots(const QList<ScreenshotEntry>& entries,const QString& destination)
{
    QStringList failures;
    for (const auto& entry : entries) {
        auto group=QString("PXL-%1-%2").arg(entry.instanceName,entry.instanceId);
        for (int c=0;c<group.size();++c) if (group[c].unicode()<32 || QStringLiteral("<>:\"/\\|?*").contains(group[c])) group[c]='_';
        group=group.left(120).trimmed();
        const auto folder=QDir(destination).filePath(group);
        if (!QDir().mkpath(folder)) { failures.append(entry.file); continue; }
        const QFileInfo source(entry.file);
        QString target=QDir(folder).filePath(source.fileName());
        int suffix=2;
        while (QFileInfo::exists(target)) target=QDir(folder).filePath(source.completeBaseName()+QString(" (%1).").arg(suffix++)+source.suffix());
        if (!QFile::copy(entry.file,target)) failures.append(entry.file);
    }
    return failures;
}
// An image-only child overlay, not a dialog or another native window.
class ScreenshotLightbox final : public QWidget {
public:
    static void showImage(const QString& file,QWidget* surface)
    {
        auto* window=surface->window();
        const QPointer<QWidget> focused=QApplication::focusWidget();
        QPointer<ScreenshotLightbox> layer=new ScreenshotLightbox(file,window);
        QList<QPointer<QWidget>> disabled;
        for (auto* sibling : window->findChildren<QWidget*>(QString(),Qt::FindDirectChildrenOnly))
            if (sibling!=layer && sibling->isEnabled()) { sibling->setEnabled(false); disabled.append(sibling); }
        QEventLoop loop;
        layer->finished=[&loop] { loop.quit(); };
        QObject::connect(layer,&QObject::destroyed,&loop,&QEventLoop::quit);
        layer->show(); layer->raise(); layer->setFocus(Qt::OtherFocusReason);
        layer->animateTo(1.0);
        loop.exec();
        if (layer) delete layer;
        for (const auto& widget : disabled) if (widget) widget->setEnabled(true);
        if (focused) focused->setFocus(Qt::OtherFocusReason);
    }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing); painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.fillRect(rect(),QColor(5,8,12,qRound(220*m_reveal)));
        if (m_image.isNull()) return;
        const auto fit=m_image.size().scaled(size()-QSize(64,64),Qt::KeepAspectRatio);
        const QSize animated=(QSizeF(fit)*(0.92+0.08*m_reveal)).toSize();
        m_imageArea=QRect(QPoint((width()-animated.width())/2,(height()-animated.height())/2),animated);
        painter.setOpacity(m_reveal);
        QPainterPath clip; clip.addRoundedRect(QRectF(m_imageArea),18,18); painter.setClipPath(clip);
        painter.drawPixmap(m_imageArea,m_image); painter.setClipping(false);
        m_closeArea=QRect(m_imageArea.right()-42,m_imageArea.top()+10,32,32);
        {
            painter.setPen(Qt::NoPen); painter.setBrush(QColor(15,19,24,200)); painter.drawEllipse(m_closeArea);
            painter.setPen(QPen(QColor("#edf0f4"),2,Qt::SolidLine,Qt::RoundCap));
            const auto center=m_closeArea.center(); painter.drawLine(center+QPoint(-5,-5),center+QPoint(5,5)); painter.drawLine(center+QPoint(-5,5),center+QPoint(5,-5));
        }
    }
    void mouseMoveEvent(QMouseEvent*) override { update(); }
    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button()==Qt::LeftButton && (!m_imageArea.contains(event->position().toPoint()) || m_closeArea.contains(event->position().toPoint()))) close();
    }
    void keyPressEvent(QKeyEvent* event) override { if (event->key()==Qt::Key_Escape) close(); else event->accept(); }
    void closeEvent(QCloseEvent* event) override
    {
        if (!m_closing && m_reveal>0 && QApplication::isEffectEnabled(Qt::UI_AnimateMenu)) {
            event->ignore(); m_closing=true; animateTo(0.0); return;
        }
        if (m_closing && m_animation->state()==QAbstractAnimation::Running) { event->ignore(); return; }
        event->accept(); hide(); if (finished) finished();
    }
    bool eventFilter(QObject* watched,QEvent* event) override
    {
        if (watched==parentWidget() && event->type()==QEvent::Resize) setGeometry(parentWidget()->rect());
        return QWidget::eventFilter(watched,event);
    }
private:
    ScreenshotLightbox(const QString& file,QWidget* parent) : QWidget(parent),m_image(QPixmap::fromImage(loadBackdropImage(file)))
    {
        setObjectName("pxlScreenshotPreview"); setGeometry(parent->rect()); parent->installEventFilter(this);
        setFocusPolicy(Qt::StrongFocus); setMouseTracking(true);
        setAccessibleName(screenshotText("Screenshot preview. Escape closes."));
        m_animation=new QVariantAnimation(this);
        m_animation->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_animation,&QVariantAnimation::valueChanged,this,[this](const QVariant& value) { m_reveal=value.toReal(); update(); });
        connect(m_animation,&QVariantAnimation::finished,this,[this] { if (m_closing) close(); });
    }
    void animateTo(qreal end)
    {
        m_animation->stop();
        if (!QApplication::isEffectEnabled(Qt::UI_AnimateMenu)) { m_reveal=end; update(); return; }
        m_animation->setDuration(end>0 ? 180 : 130);
        m_animation->setStartValue(m_reveal); m_animation->setEndValue(end); m_animation->start();
    }
    QVariantAnimation* m_animation=nullptr;
    qreal m_reveal=0.0;
    bool m_closing=false;
    QPixmap m_image;
    QRect m_imageArea,m_closeArea;
    std::function<void()> finished;
};

class ScreenshotGallery final : public QWidget {
public:
    explicit ScreenshotGallery(InstanceList* instances,QWidget* parent=nullptr,bool picking=false)
        : QWidget(parent),m_instances(instances),m_picking(picking)
    {
        setProperty("pxlNavigation","workspaceScreenshots"); setObjectName("pxlScreenshotGallery");
        auto* layout=new QVBoxLayout(this); layout->setContentsMargins(24,12,24,24); layout->setSpacing(14);
        auto* tools=new QHBoxLayout;
        m_groups=new ChoiceChip(this); m_groups->setObjectName("screenshotInstances");
        m_groups->setAccessibleName(screenshotText("Filter screenshots by instance"));
        m_groups->addItem(screenshotText("All instances"),QString());
        tools->addWidget(m_groups); tools->addStretch();
        auto* refresh=new QPushButton(screenshotText("Refresh"),this); tools->addWidget(refresh);
        layout->addLayout(tools);
        m_status=new QLabel(this); m_status->setProperty("pxlFlowHint",true); layout->addWidget(m_status);
        m_model=new ScreenshotModel(this); m_proxy=new QSortFilterProxyModel(this); m_proxy->setSourceModel(m_model); m_proxy->setFilterRole(ScreenshotModel::InstanceRole);
        m_view=new QListView(this); m_view->setObjectName("screenshotGrid"); m_view->setModel(m_proxy);
        m_view->setItemDelegate(new ScreenshotDelegate(m_view)); m_view->setViewMode(QListView::IconMode); m_view->setResizeMode(QListView::Adjust);
        m_view->setMovement(QListView::Static); m_view->setGridSize(QSize(244,190)); m_view->setUniformItemSizes(true);
        m_view->setSelectionMode(picking ? QAbstractItemView::SingleSelection : QAbstractItemView::ExtendedSelection);
        m_view->setEditTriggers(QAbstractItemView::NoEditTriggers); m_view->setStyleSheet("QListView { background: transparent; border: none; outline: none; }");
        m_view->setContextMenuPolicy(Qt::CustomContextMenu); layout->addWidget(m_view,1);
        connect(refresh,&QPushButton::clicked,this,[this] { reload(); });
        connect(m_groups,&QComboBox::currentIndexChanged,this,[this] {
            m_proxy->setFilterRegularExpression(m_groups->currentData().toString().isEmpty() ? QRegularExpression() : QRegularExpression("^"+QRegularExpression::escape(m_groups->currentData().toString())+"$")); updateCount();
        });
        connect(m_view,&QListView::customContextMenuRequested,this,[this](const QPoint& point) { contextMenu(point); });
        connect(m_view,&QListView::clicked,this,[this](const QModelIndex& index) {
            if (!m_picking && !(QApplication::keyboardModifiers() & (Qt::ControlModifier | Qt::ShiftModifier))) preview(entry(index));
        });
        m_view->installEventFilter(this);
        reload();
    }
    std::function<void()> coverChanged;
    QListView* view() const { return m_view; }
    QString selectedFile() const { const auto selected=m_view->selectionModel()->selectedRows(); return selected.isEmpty() ? QString() : entry(selected.first()).file; }
    void filterInstance(const QString& id) { const int index=m_groups->findData(id); if (index>=0) m_groups->setCurrentIndex(index); }
protected:
    bool eventFilter(QObject* watched,QEvent* event) override
    {
        if (watched==m_view && !m_picking && event->type()==QEvent::KeyPress) {
            const auto* key=static_cast<QKeyEvent*>(event);
            if (key->key()==Qt::Key_Return || key->key()==Qt::Key_Enter) {
                if (m_view->currentIndex().isValid()) preview(entry(m_view->currentIndex()));
                return true;
            }
        }
        return QWidget::eventFilter(watched,event);
    }
private:
    ScreenshotEntry entry(const QModelIndex& index) const { return m_model->entry(m_proxy->mapToSource(index).row()); }
    void updateCount() { if (m_loading) { m_status->setText(screenshotText("Loading screenshots…")); return; } m_status->setText(m_proxy->rowCount() ? screenshotText("Screenshots: %1").arg(m_proxy->rowCount()) : screenshotText("No screenshots yet. Take one in Minecraft with F2.")); }
    void reload()
    {
        if (m_loading) return;
        m_loading=true; m_status->setText(screenshotText("Loading screenshots…"));
        QList<ScreenshotEntry> roots;
        const auto previous=m_groups->currentData().toString();
        { const QSignalBlocker blocker(m_groups); m_groups->clear(); m_groups->addItem(screenshotText("All instances"),QString());
          if (m_instances) for (int i=0;i<m_instances->count();++i) {
              const auto instance=m_instances->at(i);
              if (!dynamic_cast<MinecraftInstance*>(instance)) continue;
              roots.append({{},instance->id(),instance->name(),instance->instanceRoot(),QDir(instance->gameRoot()).filePath("screenshots"),0});
              m_groups->addItem(instance->name(),instance->id());
          }
          const int current=m_groups->findData(previous); m_groups->setCurrentIndex(current<0 ? 0 : current);
        }
        auto* watcher=new QFutureWatcher<QList<ScreenshotEntry>>(this);
        connect(watcher,&QFutureWatcher<QList<ScreenshotEntry>>::finished,this,[this,watcher] {
            m_model->replace(watcher->result()); watcher->deleteLater(); m_loading=false; updateCount();
        });
        watcher->setFuture(QtConcurrent::run([roots] { return scanScreenshots(roots); }));
    }
    void preview(const ScreenshotEntry& item) { ScreenshotLightbox::showImage(item.file,this); }

    void makeBanner(const ScreenshotEntry& item,const QString& id)
    {
        const QPointer<BaseInstance> target=m_instances ? m_instances->getInstanceById(id) : nullptr; if (!target) return;
        if (!messagePanel(this,screenshotText("Set instance banner"),screenshotText("Use this screenshot as the banner for %1?").arg(target->name()),screenshotText("Use banner"),true)) return;
        if (!target) return;
        QString error; if (!saveInstanceCover(target->instanceRoot(),item.file,&error)) showNotice(this,screenshotText("Could not save banner"),error);
        else if (coverChanged) coverChanged();
    }
    void contextMenu(const QPoint& point)
    {
        const auto index=m_view->indexAt(point); if (!index.isValid()) return;
        if (!m_view->selectionModel()->isSelected(index)) m_view->selectionModel()->setCurrentIndex(index,QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        const auto item=entry(index); FrostedMenu menu(this);
        FrostedMenu targets(this);
        // exec() runs a nested event loop; navigation may destroy the gallery.
        // Keep these stack objects out of QObject parent ownership.
        menu.setParent(nullptr,menu.windowFlags()); targets.setParent(nullptr,targets.windowFlags());
        connect(this,&QObject::destroyed,&menu,&QMenu::close);
        if (!m_picking) {
            auto* own=menu.addAction(screenshotText("Make banner for this instance")); connect(own,&QAction::triggered,this,[this,item] { makeBanner(item,item.instanceId); });
            auto* other=menu.addMenu(&targets); other->setText(screenshotText("Make banner for another instance…"));
            if (m_instances) for (int i=0;i<m_instances->count();++i) {
                const auto instance=m_instances->at(i); auto* action=targets.addAction(instance->name());
                connect(action,&QAction::triggered,this,[this,item,id=instance->id()] { makeBanner(item,id); });
            }
            menu.addSeparator();
        }
        auto* copyImage=menu.addAction(screenshotText("Copy image"));
        connect(copyImage,&QAction::triggered,this,[this,item] {
            if (!copyScreenshotImage(item.file)) showNotice(this,screenshotText("Could not copy image"),screenshotText("The screenshot could not be read. The clipboard was not changed."));
        });
        auto* exportAction=menu.addAction(screenshotText("Export to folder…"));
        connect(exportAction,&QAction::triggered,this,[this] {
            const auto destination=QFileDialog::getExistingDirectory(this,screenshotText("Export screenshots to folder")); if (destination.isEmpty()) return;
            QList<ScreenshotEntry> selected; for (const auto& row : m_view->selectionModel()->selectedRows()) selected.append(entry(row));
            auto* watcher=new QFutureWatcher<QStringList>(this);
            connect(watcher,&QFutureWatcher<QStringList>::finished,this,[this,watcher] {
                const auto failures=watcher->result(); watcher->deleteLater(); showNotice(this,screenshotText("Screenshot export"),failures.isEmpty() ? screenshotText("Screenshots copied. Originals are unchanged.") : screenshotText("Could not copy these files:")+"\n"+failures.join("\n"));
            });
            watcher->setFuture(QtConcurrent::run([selected,destination] { return exportScreenshots(selected,destination); }));
        });
        auto* folder=menu.addAction(screenshotText("Open screenshots folder")); connect(folder,&QAction::triggered,this,[item] { QDesktopServices::openUrl(QUrl::fromLocalFile(item.folder)); });
        menu.exec(m_view->viewport()->mapToGlobal(point));
    }
    QPointer<InstanceList> m_instances;
    ScreenshotModel* m_model;
    QSortFilterProxyModel* m_proxy;
    QListView* m_view;
    ChoiceChip* m_groups;
    QLabel* m_status;
    bool m_picking=false,m_loading=false;
};
inline QString chooseScreenshot(QWidget* surface,InstanceList* instances)
{
    QDialog dialog(surface); dialog.setProperty("pxlFlow",true); dialog.setProperty("pxlPreferredSize",QSize(850,640));
    auto* layout=new QVBoxLayout(&dialog); layout->setContentsMargins(20,20,20,20);
    auto* title=new QLabel(screenshotText("Choose a screenshot"),&dialog); title->setProperty("pxlFlowTitle",true); layout->addWidget(title);
    auto* gallery=new ScreenshotGallery(instances,&dialog,true); layout->addWidget(gallery,1);
    auto* buttons=new QHBoxLayout; buttons->addStretch();
    auto* cancel=new QPushButton(screenshotText("Cancel"),&dialog); auto* use=new QPushButton(screenshotText("Use screenshot"),&dialog); use->setProperty("pxlPrimary",true); use->setEnabled(false);
    buttons->addWidget(cancel); buttons->addWidget(use); layout->addLayout(buttons);
    QObject::connect(gallery->view()->selectionModel(),&QItemSelectionModel::selectionChanged,&dialog,[gallery,use] { use->setEnabled(!gallery->selectedFile().isEmpty()); });
    QObject::connect(cancel,&QPushButton::clicked,&dialog,&QDialog::reject); QObject::connect(use,&QPushButton::clicked,&dialog,&QDialog::accept);
    return DialogOverlay::run(&dialog,surface->window())==QDialog::Accepted ? gallery->selectedFile() : QString();
}
} // namespace Pxl

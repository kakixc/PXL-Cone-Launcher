// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "FrostedSurface.h"
#include "MessagePanel.h"
#include "Icons.h"
#include <minecraft/auth/AccountList.h>
#include <minecraft/auth/MinecraftAccount.h>
#include <minecraft/auth/AuthFlow.h>
#include <ui/dialogs/MSALoginDialog.h>
#include <ui/dialogs/ElyLoginDialog.h>
#include <ui/dialogs/ChooseOfflineNameDialog.h>
#include <ui/dialogs/skins/SkinManageDialog.h>
#include <DesktopServices.h>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QPainterPath>
#include <functional>
#include <QToolButton>
#include <QCursor>
#include <QLineEdit>

namespace Pxl {
inline QString accountText(const char* text) { return QCoreApplication::translate("Pxl::AccountsPage",text); }

inline bool containsAccount(AccountList* accounts,const MinecraftAccountPtr& account)
{
    for (int i=0;i<accounts->count();++i) if (accounts->at(i)==account) return true;
    return false;
}
inline void removeAccountProfile(QWidget* surface,AccountList* accounts,const MinecraftAccountPtr& account)
{
    if (!containsAccount(accounts,account) || account->isActive() || account->isInUse()) return;
    const QPointer<QWidget> alive(surface); const QPointer<AccountList> model(accounts);
    if (!messagePanel(surface,accountText("Remove profile?"),accountText("Remove %1 from this launcher? Your Minecraft files will stay on this computer.").arg(account->profileName()),accountText("Remove"),true) || !alive || !model) return;
    if (account->isActive() || account->isInUse()) return;
    for (int i=0;i<model->count();++i) if (model->at(i)==account) { model->removeAccount(model->index(i,0)); break; }
}
class ProfileHoverActions final : public QObject {
public:
    ProfileHoverActions(QWidget* row,QWidget* slot,bool position,const std::function<void()>& edit,const std::function<void()>& remove,bool enabled)
        : QObject(row),m_row(row),m_slot(slot),m_position(position)
    {
        auto* layout=new QHBoxLayout(slot); layout->setContentsMargins(0,0,0,0); layout->setSpacing(4);
        for (bool deleting : {false,true}) {
            auto* action=new QToolButton(slot); action->setObjectName(deleting ? "pxlRemoveProfile" : "pxlEditProfile");
            action->setIcon(QIcon(deleting ? ":/pxl/icons/trash.svg" : ":/pxl/icons/pencil.svg")); action->setFixedSize(28,28); action->setIconSize(QSize(17,17));
            action->setToolTip(deleting ? accountText("Remove profile") : accountText("Edit profile")); action->setAccessibleName(action->toolTip()); action->setEnabled(enabled);
            action->setStyleSheet("QToolButton { background: transparent; border: none; border-radius: 8px; padding: 0; } QToolButton:hover { background: rgba(255,255,255,30); }");
            connect(action,&QToolButton::clicked,row,deleting ? remove : edit); layout->addWidget(action);
        }
        slot->setFixedSize(60,28); slot->hide(); row->installEventFilter(this);
        arrange();
    }
protected:
    bool eventFilter(QObject*,QEvent* event) override
    {
        if (event->type()==QEvent::Resize) arrange();
        if (event->type()==QEvent::Enter || event->type()==QEvent::FocusIn) m_slot->show();
        if (event->type()==QEvent::Leave) QTimer::singleShot(0,this,[this] {
            if (!m_row->rect().contains(m_row->mapFromGlobal(QCursor::pos())) && !m_row->hasFocus()) m_slot->hide();
        });
        return false;
    }
private:
    void arrange() { if (m_position) m_slot->move(m_row->width()-68,(m_row->height()-28)/2); }
    QWidget* m_row; QWidget* m_slot; bool m_position;
};

// A bounded quick switcher; the add action stays outside the scrolling list.
class AccountPopup final : public QWidget {
public:
    AccountPopup(AccountList* accounts,QWidget* surface,const QPoint& anchor,std::function<void()> manage,
                 std::function<void(MinecraftAccountPtr)> edit={},std::function<void(MinecraftAccountPtr)> remove={})
        : QWidget(surface,Qt::Popup|Qt::FramelessWindowHint|Qt::NoDropShadowWindowHint),m_surface(surface)
    {
        setObjectName("pxlAccountPopup");
        setAttribute(Qt::WA_DeleteOnClose); setAttribute(Qt::WA_TranslucentBackground); setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_NoMouseReplay); setAutoFillBackground(false);
        auto* layout=new QVBoxLayout(this); layout->setContentsMargins(8,8,8,8); layout->setSpacing(6);
        auto* scroll=new QScrollArea(this); scroll->setObjectName("pxlAccountChoices");
        scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame); scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setStyleSheet("QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; border: none; }");
        auto* content=new QWidget(scroll); auto* rows=new QVBoxLayout(content); rows->setContentsMargins(0,0,0,0); rows->setSpacing(3);
        for (int i=0;i<accounts->count();++i) {
            const auto account=accounts->at(i);
            auto* choice=new QPushButton(account->profileName().isEmpty() ? account->displayName() : account->profileName(),content);
            choice->setObjectName("pxlQuickProfile"); choice->setFixedHeight(44); choice->setCheckable(true);
            choice->setChecked(accounts->defaultAccount()==account); choice->setEnabled(!account->isActive());
            choice->setIcon(account->getFace().isNull() ? QIcon(":/pxl/icons/user-circle.svg") : QIcon(account->getFace()));
            choice->setIconSize(QSize(28,28)); choice->setCursor(Qt::PointingHandCursor);
            choice->setStyleSheet("QPushButton { text-align: left; color: #edf0f4; background: transparent; border: none; border-radius: 10px; padding: 6px 10px; }"
                                 "QPushButton:hover { background: rgba(255,255,255,24); } QPushButton:checked { background: rgba(242,163,101,32); color: #f5bb90; }"
                                 "QPushButton:disabled { color: #7e8793; }");
            if (edit && remove) {
                auto* slot=new QWidget(choice);
                choice->setStyleSheet(choice->styleSheet()+"QPushButton { padding-right: 72px; }");
                new ProfileHoverActions(choice,slot,true,[this,account,edit] { close(); edit(account); },[this,account,remove] { close(); remove(account); },!account->isActive() && !account->isInUse());
            }
            rows->addWidget(choice);
            connect(choice,&QPushButton::clicked,this,[this,accounts,account] {
                close();
                for (int row=0;row<accounts->count();++row)
                    if (accounts->at(row)==account && !account->isActive()) { accounts->setDefaultAccount(account); break; }
            });
        }
        rows->addStretch(); scroll->setWidget(content); layout->addWidget(scroll,1); scroll->setVisible(accounts->count()>0);
        auto* add=new QPushButton(QIcon(":/pxl/icons/plus.svg"),accountText("Add account"),this);
        add->setObjectName("pxlQuickAddAccount"); add->setFixedHeight(38); add->setCursor(Qt::PointingHandCursor);
        add->setStyleSheet("QPushButton { color: #edf0f4; background: rgba(255,255,255,14); border: none; border-radius: 10px; padding: 6px 12px; font-weight: 600; }"
                          "QPushButton:hover { color: #f5bb90; background: rgba(242,163,101,32); }");
        layout->addWidget(add);
        connect(add,&QPushButton::clicked,this,[this,manage] { close(); manage(); });
        connect(accounts,&AccountList::listChanged,this,&QWidget::close);
        connect(accounts,&AccountList::listActivityChanged,this,&QWidget::close);
        connect(accounts,&QObject::destroyed,this,&QWidget::close);
        const auto area=QRect(surface->mapToGlobal(QPoint()),surface->size()).adjusted(8,8,-8,-8);
        resize(qMin(280,area.width()),qMin(qMin(360,accounts->count()*47+60),area.height()));
        move(qBound(area.left(),anchor.x()-width(),area.right()-width()+1),qBound(area.top(),anchor.y(),area.bottom()-height()+1));
    }
protected:
    void showEvent(QShowEvent* event) override
    {
        QPainterPath shape; shape.addRoundedRect(QRectF(rect()),14,14); setMask(QRegion(shape.toFillPolygon().toPolygon()));
        m_backdrop=frostedBackdrop(m_surface,QRect(pos(),size())); QWidget::showEvent(event);
    }
    void paintEvent(QPaintEvent*) override { QPainter painter(this); paintFrostedPanel(painter,rect(),m_backdrop); }
private:
    QPointer<QWidget> m_surface;
    QPixmap m_backdrop;
};

// PXL presentation over the existing account model and authentication services.
class AccountsPage final : public QWidget {
public:
    explicit AccountsPage(AccountList* accounts, bool supportsMicrosoft, QWidget* parent=nullptr, bool setup=false)
        : QWidget(parent), m_accounts(accounts), m_setup(setup)
    {
        setObjectName("pxlAccountsPage");
        setProperty("pxlNavigation","workspaceAccounts");
        setProperty("pxlEmbeddedAuth",true);
        auto* layout=new QVBoxLayout(this);
        layout->setContentsMargins(setup ? 0 : 28,setup ? 0 : 8,setup ? 0 : 28,setup ? 0 : 24); layout->setSpacing(18);
        auto* hint=new QLabel(accountText("Choose the profile used to launch Minecraft. Your accounts stay on this computer."),this);
        hint->setWordWrap(true); hint->setProperty("pxlFlowHint",true); layout->addWidget(hint);
        hint->setVisible(!setup);
        auto* actions=new QBoxLayout(setup ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
        actions->setSpacing(setup ? 10 : 8);
        auto* microsoft=button(accountText("Sign in with Microsoft"),"user-circle");
        microsoft->setVisible(supportsMicrosoft); actions->addWidget(microsoft);
        auto* ely=button(accountText("Sign in with Ely.by"),"user-circle"); actions->addWidget(ely);
        auto* offline=button(accountText("Offline profile"),"plus"); actions->addWidget(offline);
        if (setup) { microsoft->setObjectName("welcomeMicrosoft"); ely->setObjectName("welcomeEly"); offline->setObjectName("welcomeOffline"); }
        if (setup) for (auto* action : {microsoft, ely, offline}) {
            action->setMinimumHeight(62); action->setIconSize(QSize(24,24));
            action->setStyleSheet(action->styleSheet()+"QPushButton { text-align:left; padding:12px 18px; font-size:15px; }");
        }
        actions->addStretch(); layout->addLayout(actions);
        auto* scroll=new QScrollArea(this); m_profilesScroll=scroll; scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame);
        scroll->setStyleSheet("QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; border: none; }");
        m_content=new QWidget(scroll); m_rows=new QVBoxLayout(m_content); m_rows->setContentsMargins(0,0,0,0); m_rows->setSpacing(10);
        scroll->setWidget(m_content); layout->addWidget(scroll,1);
        connect(microsoft,&QPushButton::clicked,this,[this] { const QPointer<AccountsPage> alive(this); auto account=MSALoginDialog::newAccount(this); if (alive) add(account); });
        connect(ely,&QPushButton::clicked,this,[this] { const QPointer<AccountsPage> alive(this); auto account=ElyLoginDialog::newAccount(this); if (alive) add(account); });
        connect(offline,&QPushButton::clicked,this,[this] { addOffline(); });
        const auto schedule=[this] { if (m_refreshPending) return; m_refreshPending=true; QTimer::singleShot(0,this,[this] { m_refreshPending=false; rebuild(); }); };
        connect(accounts,&AccountList::listChanged,this,schedule);
        connect(accounts,&AccountList::listActivityChanged,this,schedule);
        connect(accounts,&AccountList::defaultAccountChanged,this,schedule);
        connect(accounts,&QAbstractItemModel::dataChanged,this,schedule);
        rebuild();
    }
    void editAccount(const MinecraftAccountPtr& account)
    {
        if (!contains(account) || account->isActive() || account->isInUse()) return;
        if (account->accountType()==AccountType::Offline) {
            ChooseOfflineNameDialog dialog(accountText("Change your offline profile name. A different name creates a different Minecraft player identity; worlds may show a different inventory."),this);
            dialog.setUsername(account->profileName()); dialog.setProperty("pxlFlow",true); dialog.setProperty("pxlPreferredSize",QSize(520,300));
            auto* heading = new QLabel(accountText("Edit offline profile"), &dialog);
            heading->setProperty("pxlFlowTitle", true);
            static_cast<QVBoxLayout*>(dialog.layout())->insertWidget(0, heading);
            const QPointer<AccountsPage> alive(this);
            if (DialogOverlay::run(&dialog,window())!=QDialog::Accepted || !alive || !contains(account)) return;
            const auto name=dialog.getUsername();
            const auto existing=m_accounts->getAccountByProfileName(name);
            if (existing && existing!=account) { showNotice(this,accountText("Edit profile"),accountText("A profile with this name already exists.")); return; }
            account->renameOfflineProfile(name);
            return;
        }
        QDialog panel(this); panel.setProperty("pxlFlow",true); panel.setProperty("pxlPreferredSize",QSize(560,300));
        auto* layout=new QVBoxLayout(&panel); layout->setContentsMargins(24,24,24,24); layout->setSpacing(16);
        auto* title=new QLabel(accountText("Edit profile"),&panel); title->setProperty("pxlFlowTitle",true); layout->addWidget(title);
        auto* name=new QLineEdit(account->profileName(),&panel); name->setReadOnly(true); layout->addWidget(name);
        auto* hint=new QLabel(accountText("Online profile names are managed on your account provider's website."),&panel); hint->setWordWrap(true); hint->setProperty("pxlFlowHint",true); layout->addWidget(hint);
        auto* actions=new QHBoxLayout;
        auto* site=new QPushButton(accountText("Open profile website"),&panel); actions->addWidget(site);
        auto* skin=new QPushButton(accountText("Manage skin"),&panel); actions->addWidget(skin);
        auto* done=new QPushButton(accountText("Done"),&panel); actions->addStretch(); actions->addWidget(done); layout->addLayout(actions);
        connect(site,&QPushButton::clicked,&panel,[account] { DesktopServices::openUrl(QUrl(account->accountType()==AccountType::MSA ? "https://www.minecraft.net/msaprofile" : "https://account.ely.by/")); });
        const QPointer<AccountList> accounts(m_accounts);
        connect(skin,&QPushButton::clicked,&panel,[&panel,account,accounts] {
            if (!accounts || !containsAccount(accounts,account) || account->isActive() || account->isInUse()) return;
            if (account->accountType()==AccountType::MSA) { SkinManageDialog dialog(&panel,account); dialog.setProperty("pxlFlow",true); dialog.setProperty("pxlPreferredSize",QSize(760,600)); DialogOverlay::run(&dialog,panel.window()); }
            else DesktopServices::openUrl(QUrl("https://ely.by/skins"));
        });
        connect(done,&QPushButton::clicked,&panel,&QDialog::accept);
        DialogOverlay::run(&panel,window());
    }
private:
    QPushButton* button(const QString& text,const QString& icon)
    {
        auto* result=new QPushButton(icon.isEmpty() ? QIcon() : QIcon(":/pxl/icons/"+icon+".svg"),text,this);
        result->setMinimumHeight(38); result->setCursor(Qt::PointingHandCursor);
        result->setStyleSheet("QPushButton { color: #e8ebef; background: rgba(57,58,63,160); border: 1px solid rgba(200,210,225,30); border-radius: 11px; padding: 6px 14px; font-weight: 600; }"
                             "QPushButton:hover { background: rgba(84,68,54,215); border-color: rgba(242,163,101,130); }"
                             "QPushButton:disabled { color: #78818d; background: rgba(40,43,48,140); }");
        return result;
    }
    bool contains(const MinecraftAccountPtr& account) const
    {
        for (int i=0;i<m_accounts->count();++i) if (m_accounts->at(i)==account) return true;
        return false;
    }
    void add(const MinecraftAccountPtr& account)
    {
        if (!account) return;
        m_accounts->addAccount(account);
        if (m_accounts->count()==1) m_accounts->setDefaultAccount(account);
    }
    void addOffline()
    {
        if (!m_accounts->anyAccountIsValid()) {
            showNotice(this,accountText("Offline profile"),accountText("Add an account that owns Minecraft before creating an offline profile."));
            return;
        }
        ChooseOfflineNameDialog dialog(accountText("Choose a name for your offline profile."),this);
        dialog.setProperty("pxlFlow",true); dialog.setProperty("pxlPreferredSize",QSize(520,240));
        const QPointer<AccountsPage> alive(this);
        if (DialogOverlay::run(&dialog,window())!=QDialog::Accepted || !alive) return;
        auto account=MinecraftAccount::createOffline(dialog.getUsername());
        if (account) { account->login()->start(); add(account); }
    }
    void rebuild()
    {
        if (m_setup) m_profilesScroll->setVisible(!m_accounts->isEmpty());
        while (auto* item=m_rows->takeAt(0)) { if (item->widget()) item->widget()->deleteLater(); delete item; }
        if (m_accounts->isEmpty()) {
            if (m_setup) { m_rows->addStretch(); return; }
            m_rows->addStretch();
            auto* icon=new QLabel(m_content); icon->setAlignment(Qt::AlignCenter); icon->setPixmap(QIcon(":/pxl/icons/user-circle.svg").pixmap(52,52)); m_rows->addWidget(icon);
            auto* title=new QLabel(accountText("Your adventure starts with a profile"),m_content); title->setAlignment(Qt::AlignCenter);
            title->setStyleSheet("font-size: 20px; font-weight: 600;"); m_rows->addWidget(title);
            auto* hint=new QLabel(accountText("Sign in above to add your first account."),m_content); hint->setAlignment(Qt::AlignCenter); hint->setProperty("pxlFlowHint",true); m_rows->addWidget(hint);
            m_rows->addStretch(); return;
        }
        for (int i=0;i<m_accounts->count();++i) {
            const auto account=m_accounts->at(i);
            const bool current=m_accounts->defaultAccount()==account;
            auto* row=new QFrame(m_content); row->setObjectName("pxlAccountRow");
            row->setStyleSheet(QString("QFrame#pxlAccountRow { background: rgba(32,36,42,190); border: 1px solid %1; border-radius: 16px; }").arg(current ? "rgba(242,163,101,115)" : "rgba(200,210,225,28)"));
            auto* line=new QHBoxLayout(row); line->setContentsMargins(18,16,18,16); line->setSpacing(16);
            auto* avatar=new QLabel(row); avatar->setFixedSize(48,48);
            auto face=account->getFace(48,48);
            if (face.isNull()) face=QIcon(":/pxl/icons/user-circle.svg").pixmap(48,48);
            QPixmap rounded(48,48); rounded.fill(Qt::transparent);
            { QPainter painter(&rounded); painter.setRenderHint(QPainter::Antialiasing); QPainterPath clip; clip.addRoundedRect(QRectF(0,0,48,48),12,12); painter.setClipPath(clip); painter.drawPixmap(rounded.rect(),face); }
            avatar->setPixmap(rounded); line->addWidget(avatar);
            auto* details=new QVBoxLayout; details->setSpacing(5);
            auto* name=new QLabel(account->profileName().isEmpty() ? account->displayName() : account->profileName(),row); name->setTextFormat(Qt::PlainText); name->setStyleSheet("font-size: 16px; font-weight: 600;"); details->addWidget(name);
            const QString type=account->accountType()==AccountType::MSA ? QStringLiteral("Microsoft") : account->accountType()==AccountType::Ely ? QStringLiteral("Ely.by") : accountText("Offline");
            const auto status=m_accounts->index(i,AccountList::StatusColumn).data().toString();
            auto* subtitle=new QLabel(type+QStringLiteral(" · ")+status,row); subtitle->setTextFormat(Qt::PlainText); subtitle->setProperty("pxlFlowHint",true); details->addWidget(subtitle);
            line->addLayout(details,1);
            auto* use=button(current ? accountText("Active profile") : accountText("Use profile"),current ? "check" : "user-circle");
            use->setObjectName("pxlUseProfile"); use->setEnabled(!current && !account->isActive()); line->addWidget(use);
            connect(use,&QPushButton::clicked,this,[this,account] { if (contains(account) && !account->isActive()) m_accounts->setDefaultAccount(account); });
            auto* slot=new QWidget(row);
            new ProfileHoverActions(row,slot,false,[this,account] { editAccount(account); },[this,account] { removeAccountProfile(this,m_accounts,account); },!account->isActive() && !account->isInUse());
            auto* holder=new QWidget(row); holder->setFixedSize(60,28); slot->setParent(holder); line->addWidget(holder);
            auto* more=button(QStringLiteral("•••"),QString()); more->setAccessibleName(accountText("Profile actions")); line->addWidget(more);
            connect(more,&QPushButton::clicked,this,[this,account,more] { showActions(account,more); });
            m_rows->addWidget(row);
        }
        m_rows->addStretch();
    }
    void showActions(const MinecraftAccountPtr& account,QWidget* anchor)
    {
        FrostedMenu menu(this);
        // The menu is stack-owned and can outlive its source page during nested events.
        const auto flags=menu.windowFlags(); menu.setParent(nullptr,flags);
        connect(this,&QObject::destroyed,&menu,&QMenu::close);
        const bool ready=!account->isActive();
        auto addAction=[&](const QString& label,const QString& icon,bool enabled,auto callback) {
            auto* action=menu.addAction(QIcon(":/pxl/icons/"+icon+".svg"),label); action->setEnabled(enabled);
            connect(action,&QAction::triggered,this,[this,account,callback] { if (contains(account) && !account->isActive()) callback(); });
        };
        addAction(accountText("Edit profile"),"pencil",ready && !account->isInUse(),[this,account] { editAccount(account); });
        addAction(accountText("Refresh session"),"arrows-clockwise",ready && account->accountType()!=AccountType::Offline,[this,account] { m_accounts->requestRefresh(account->internalId()); });
        addAction(accountText("Manage skin"),"user-circle",ready && !account->isInUse() && account->accountType()!=AccountType::Offline,[this,account] {
            if (account->accountType()==AccountType::MSA) { SkinManageDialog dialog(this,account); dialog.setProperty("pxlFlow",true); dialog.setProperty("pxlPreferredSize",QSize(760,600)); DialogOverlay::run(&dialog,window()); }
            else DesktopServices::openUrl(QUrl("https://ely.by/skins"));
        });
        addAction(accountText("Clear active profile"),"user-circle",ready && m_accounts->defaultAccount()==account,[this] { m_accounts->setDefaultAccount(nullptr); });
        int index=-1; for (int i=0;i<m_accounts->count();++i) if (m_accounts->at(i)==account) index=i;
        for (int delta : {-1,1}) addAction(delta<0 ? accountText("Move up") : accountText("Move down"),"sliders-horizontal",ready && index+delta>=0 && index+delta<m_accounts->count(),[this,account,delta] {
            for (int i=0;i<m_accounts->count();++i) if (m_accounts->at(i)==account) { m_accounts->moveAccount(m_accounts->index(i,0),delta); break; }
        });
        menu.addSeparator();
        addAction(accountText("Remove profile"),"trash",ready && !account->isInUse(),[this,account] { removeAccountProfile(this,m_accounts,account); });
        menu.exec(anchor->mapToGlobal(QPoint(0,anchor->height()+4)));
    }
    AccountList* m_accounts;
    QWidget* m_content;
    QVBoxLayout* m_rows;
    bool m_refreshPending=false;
    bool m_setup=false;
    QScrollArea* m_profilesScroll=nullptr;
};
} // namespace Pxl

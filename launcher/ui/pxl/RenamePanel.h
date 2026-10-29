// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "DialogOverlay.h"
#include "ToggleSwitch.h"
#include <QCheckBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <optional>
namespace Pxl {
inline QString renameText(const char* source) { return QCoreApplication::translate("Pxl::RenamePanel",source); }
inline std::optional<QString> renameInstance(QWidget* surface,const QString& current)
{
    QDialog panel(surface); panel.setObjectName("pxlRenameInstance"); panel.setProperty("pxlFlow",true);
    panel.setProperty("pxlPreferredSize",QSize(520,240));
    auto* layout=new QVBoxLayout(&panel); layout->setContentsMargins(24,24,24,24); layout->setSpacing(16);
    auto* title=new QLabel(renameText("Rename instance"),&panel); title->setProperty("pxlFlowTitle",true); layout->addWidget(title);
    auto* name=new QLineEdit(current,&panel); name->setObjectName("instanceNewName");
    name->setAccessibleName(renameText("Instance name")); name->setMaxLength(128); name->setMinimumHeight(42); layout->addWidget(name);
    layout->addStretch(); auto* actions=new QHBoxLayout; actions->addStretch();
    auto* cancel=new QPushButton(renameText("Cancel"),&panel); actions->addWidget(cancel);
    auto* save=new QPushButton(renameText("Rename"),&panel); save->setProperty("pxlPrimary",true); save->setDefault(true); actions->addWidget(save);
    layout->addLayout(actions);
    QObject::connect(cancel,&QPushButton::clicked,&panel,&QDialog::reject);
    QObject::connect(save,&QPushButton::clicked,&panel,&QDialog::accept);
    QObject::connect(name,&QLineEdit::textChanged,save,[save](const QString& text) { save->setEnabled(!text.trimmed().isEmpty()); });
    name->selectAll(); QTimer::singleShot(0,name,[name] { name->setFocus(); name->selectAll(); });
    if (DialogOverlay::run(&panel,surface->window())!=QDialog::Accepted) return std::nullopt;
    return name->text().trimmed();
}
inline bool confirmFolderRename(QWidget* surface,const QString& before,const QString& after,bool& remember)
{
    QDialog panel(surface); panel.setProperty("pxlFlow",true); panel.setObjectName("pxlRenameFolder");
    panel.setProperty("pxlPreferredSize",QSize(560,320));
    auto* layout=new QVBoxLayout(&panel); layout->setContentsMargins(24,24,24,24); layout->setSpacing(16);
    auto* title=new QLabel(renameText("Rename the folder too?"),&panel); title->setProperty("pxlFlowTitle",true); layout->addWidget(title);
    auto* hint=new QLabel(renameText("The instance name has changed. You can keep its current folder name."),&panel);
    hint->setWordWrap(true); layout->addWidget(hint);
    auto* names=new QLabel(before+"\n→ "+after,&panel); names->setTextFormat(Qt::PlainText); names->setWordWrap(true); layout->addWidget(names);
    auto* choice=new QCheckBox(renameText("Remember my choice"),&panel); layout->addWidget(choice); layout->addStretch();
    auto* actions=new QHBoxLayout; actions->addStretch();
    auto* keep=new QPushButton(renameText("Keep folder name"),&panel); keep->setDefault(true); actions->addWidget(keep);
    auto* rename=new QPushButton(renameText("Rename folder"),&panel); rename->setProperty("pxlPrimary",true); actions->addWidget(rename);
    layout->addLayout(actions);
    QObject::connect(keep,&QPushButton::clicked,&panel,&QDialog::reject);
    QObject::connect(rename,&QPushButton::clicked,&panel,&QDialog::accept);
    const bool accepted=DialogOverlay::run(&panel,surface->window())==QDialog::Accepted;
    remember=choice->isChecked(); return accepted;
}
}

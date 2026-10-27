// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "DialogOverlay.h"
#include "InstanceCovers.h"
#include "MessagePanel.h"
#include "ScreenshotGallery.h"
#include <QDialog>
#include <QFileDialog>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace Pxl {
inline bool editInstanceCover(QWidget* surface, const QString& root, InstanceList* instances = nullptr)
{
    auto tr = [](const char* text) { return QCoreApplication::translate("Pxl::CoverPicker",text); };
    QDialog panel(surface);
    panel.setObjectName("pxlCoverPicker");
    panel.setProperty("pxlFlow",true);
    panel.setProperty("pxlPreferredSize",QSize(620,560));
    auto* layout = new QVBoxLayout(&panel);
    layout->setContentsMargins(24,24,24,24);
    layout->setSpacing(14);
    auto* title = new QLabel(tr("Instance cover"),&panel);
    title->setProperty("pxlFlowTitle",true);
    layout->addWidget(title);
    auto* preview = new QLabel(&panel);
    preview->setObjectName("pxlCoverPreview");
    preview->setFixedHeight(146);
    preview->setAlignment(Qt::AlignCenter);
    preview->setStyleSheet("QLabel { background: #353b40; border-radius: 12px; }");
    layout->addWidget(preview);
    QImage selected = instanceCover(root).toImage();
    bool neutral = selected.isNull();
    bool changed = false;
    auto updatePreview = [&] {
        QPixmap image(560,146); image.fill(QColor("#353b40"));
        if (!neutral && !selected.isNull()) {
            const auto cover = QPixmap::fromImage(selected).scaled(image.size(),Qt::KeepAspectRatioByExpanding,Qt::SmoothTransformation);
            QPainter painter(&image);
            painter.drawPixmap((image.width()-cover.width())/2,(image.height()-cover.height())/2,cover);
        }
        QPixmap rounded(image.size()); rounded.fill(Qt::transparent);
        QPainter painter(&rounded); painter.setRenderHint(QPainter::Antialiasing);
        QPainterPath clip; clip.addRoundedRect(QRectF(rounded.rect()),12,12); painter.setClipPath(clip); painter.drawPixmap(0,0,image);
        preview->setPixmap(rounded);
    };
    updatePreview();
    auto* upload = new QPushButton(tr("Choose your image…"),&panel);
    upload->setIcon(QIcon(":/pxl/icons/import.svg"));
    layout->addWidget(upload);
    QObject::connect(upload,&QPushButton::clicked,&panel,[&] {
        const auto file = QFileDialog::getOpenFileName(&panel,tr("Choose your image…"),{},tr("Images (*.png *.jpg *.jpeg *.webp *.bmp)"));
        if (file.isEmpty()) return;
        const auto image = loadBackdropImage(file);
        if (image.isNull()) { showNotice(&panel,tr("Could not open image"),tr("Choose a valid image up to 16384 pixels and 64 megapixels.")); return; }
        selected = image; neutral = false; changed = true; updatePreview();
    });
    if (instances) {
        auto* screenshots=new QPushButton(tr("From screenshots…"),&panel);
        screenshots->setIcon(QIcon(":/pxl/icons/image.svg"));
        layout->addWidget(screenshots);
        QObject::connect(screenshots,&QPushButton::clicked,&panel,[&] {
            const auto file=chooseScreenshot(&panel,instances);
            if (file.isEmpty()) return;
            const auto image=loadBackdropImage(file);
            if (image.isNull()) return;
            selected=image; neutral=false; changed=true; updatePreview();
        });
    }
    auto* choices = new QGridLayout;
    choices->setSpacing(10);
    const QList<QPair<QString,QString>> presets{{tr("Neutral"),QString()}, {tr("PXL dawn"),":/pxl/cone-dawn-banner.png"},
        {tr("PXL valley"),":/pxl/wallpaper-source.png"}, {tr("Slate"),"#35404a"}, {tr("Warm stone"),"#655147"}, {tr("Forest"),"#36524b"}};
    QList<QPushButton*> buttons;
    for (int i=0;i<presets.size();++i) {
        const auto value = presets[i].second;
        QImage thumbnail;
        if (value.startsWith('#') || value.isEmpty()) {
            thumbnail = QImage(160,56,QImage::Format_RGB32);
            thumbnail.fill(value.isEmpty() ? QColor("#353b40") : QColor(value));
        } else thumbnail = loadBackdropImage(value);
        auto* button = new QPushButton(presets[i].first,&panel);
        button->setCheckable(true);
        button->setMinimumHeight(64);
        button->setIcon(QIcon(QPixmap::fromImage(thumbnail)));
        button->setIconSize(QSize(50,42));
        button->setProperty("pxlMode",true);
        choices->addWidget(button,i/3,i%3);
        buttons.append(button);
        QObject::connect(button,&QPushButton::clicked,&panel,[&,value,thumbnail,button] {
            for (auto* other : buttons) other->setChecked(other==button);
            neutral = value.isEmpty(); selected = thumbnail; changed = true; updatePreview();
        });
    }
    layout->addLayout(choices);
    layout->addStretch();
    auto* actions = new QHBoxLayout;
    actions->addStretch();
    auto* cancel = new QPushButton(tr("Cancel"),&panel);
    auto* apply = new QPushButton(tr("Apply"),&panel);
    apply->setProperty("pxlPrimary",true);
    actions->addWidget(cancel); actions->addWidget(apply); layout->addLayout(actions);
    QObject::connect(cancel,&QPushButton::clicked,&panel,&QDialog::reject);
    QObject::connect(apply,&QPushButton::clicked,&panel,[&] {
        if (!changed) { panel.accept(); return; }
        QString error;
        const bool ok = neutral ? resetInstanceCover(root) : writeInstanceCover(root,selected,&error);
        if (!ok) { showNotice(&panel,tr("Could not save cover"),error.isEmpty() ? tr("Could not remove the saved cover.") : error); return; }
        panel.accept();
    });
    return DialogOverlay::run(&panel,surface->window())==QDialog::Accepted;
}
} // namespace Pxl

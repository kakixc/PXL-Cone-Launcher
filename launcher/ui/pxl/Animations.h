// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QApplication>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QWidget>

namespace Pxl {
inline void animatePage(QWidget* page)
{
    if (!page || !page->isVisible() || !QApplication::isEffectEnabled(Qt::UI_AnimateMenu)) return;
    auto* effect = qobject_cast<QGraphicsOpacityEffect*>(page->graphicsEffect());
    if (page->graphicsEffect() && !effect) return;
    if (!effect) {
        effect = new QGraphicsOpacityEffect(page);
        page->setGraphicsEffect(effect);
    }
    auto* animation = page->findChild<QPropertyAnimation*>("pxlPageFade", Qt::FindDirectChildrenOnly);
    if (!animation) {
        animation = new QPropertyAnimation(effect, "opacity", page);
        animation->setObjectName("pxlPageFade");
        animation->setDuration(150);
        animation->setEasingCurve(QEasingCurve::OutCubic);
        QObject::connect(animation, &QPropertyAnimation::finished, effect, [effect] {
            effect->setOpacity(1.0);
            effect->setEnabled(false);
        });
    }
    animation->stop();
    effect->setEnabled(true);
    animation->setStartValue(0.45);
    animation->setEndValue(1.0);
    animation->start();
}
} // namespace Pxl

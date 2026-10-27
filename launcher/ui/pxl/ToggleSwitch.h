// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QApplication>
#include <QCheckBox>
#include <QPainter>
#include <QVariantAnimation>

namespace Pxl {
// Keep QCheckBox keyboard/accessibility semantics with an opt-in PXL presentation.
class ToggleSwitch final : public QCheckBox {
public:
    explicit ToggleSwitch(const QString& text,QWidget* parent=nullptr) : QCheckBox(text,parent)
    {
        setCursor(Qt::PointingHandCursor); setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed); setMinimumHeight(42);
        setToolTip(text); setFocusPolicy(Qt::StrongFocus);
        m_animation=new QVariantAnimation(this); m_animation->setDuration(140); m_animation->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_animation,&QVariantAnimation::valueChanged,this,[this](const QVariant& value) { m_position=value.toReal(); update(); });
        connect(this,&QCheckBox::toggled,this,[this](bool checked) {
            m_animation->stop();
            if (!isVisible() || !QApplication::isEffectEnabled(Qt::UI_AnimateMenu)) { m_position=checked ? 1.0 : 0.0; update(); return; }
            m_animation->setStartValue(m_position); m_animation->setEndValue(checked ? 1.0 : 0.0); m_animation->start();
        });
    }
    QSize minimumSizeHint() const override { return {160,42}; }
    QSize sizeHint() const override { return {fontMetrics().horizontalAdvance(text())+80,44}; }
protected:
    bool hitButton(const QPoint& point) const override { return rect().contains(point); }
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing);
        if (!isEnabled()) painter.setOpacity(.45);
        if (underMouse() && isEnabled()) { painter.setPen(Qt::NoPen); painter.setBrush(QColor(255,255,255,8)); painter.drawRoundedRect(rect(),9,9); }
        const bool rtl=layoutDirection()==Qt::RightToLeft;
        const QRect track(rtl ? 6 : width()-48,(height()-22)/2,40,22);
        const auto off=QColor("#535b67"),on=QColor("#d29463");
        auto blend=[this](int a,int b) { return qRound(a+(b-a)*m_position); };
        painter.setPen(Qt::NoPen); painter.setBrush(QColor(blend(off.red(),on.red()),blend(off.green(),on.green()),blend(off.blue(),on.blue())));
        painter.drawRoundedRect(track,11,11);
        painter.setBrush(isChecked() ? QColor("#251c15") : QColor("#e0e5ec"));
        painter.drawEllipse(QRectF(track.left()+3+18*(rtl ? 1.0-m_position : m_position),track.top()+3,16,16));
        painter.setPen(QColor("#edf0f4")); painter.setFont(font());
        const QRect label=rtl ? rect().adjusted(62,0,-6,0) : rect().adjusted(6,0,-62,0);
        painter.drawText(label,Qt::AlignVCenter|(rtl ? Qt::AlignRight : Qt::AlignLeft),fontMetrics().elidedText(text(),Qt::ElideRight,label.width()));
        if (hasFocus()) { painter.setBrush(Qt::NoBrush); painter.setPen(QColor(242,163,101,160)); painter.drawRoundedRect(QRectF(rect()).adjusted(1,1,-1,-1),9,9); }
    }
    void enterEvent(QEnterEvent* event) override { QCheckBox::enterEvent(event); update(); }
    void leaveEvent(QEvent* event) override { QCheckBox::leaveEvent(event); update(); }
private:
    QVariantAnimation* m_animation;
    qreal m_position=0;
};
} // namespace Pxl

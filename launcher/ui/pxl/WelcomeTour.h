// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QApplication>
#include <QEasingCurve>
#include <QHideEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QVariantAnimation>
#include <QWidget>
#include <cmath>
#include <functional>

namespace Pxl {
inline QString welcomeText(const char* text) { return QCoreApplication::translate("Pxl::WelcomeSetup", text); }

// Illustrative, read-only miniatures. No account, download or filesystem actions.
// One short playback per selection; hidden previews never keep animating.
class WelcomeTour final : public QWidget {
public:
    explicit WelcomeTour(QWidget* parent = nullptr) : QWidget(parent)
    {
        setObjectName("pxlWelcomeTour");
        setMinimumHeight(160);
        auto policy = QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        policy.setHeightForWidth(true); setSizePolicy(policy);
        m_animation = new QVariantAnimation(this);
        m_animation->setDuration(5600);
        m_animation->setStartValue(0.0);
        m_animation->setEndValue(1.0);
        connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
            m_progress = value.toReal(); update();
            if (progressChanged) progressChanged(m_progress);
        });
        connect(m_animation, &QVariantAnimation::finished, this, [this] { if (finished) finished(); });
    }
    std::function<void(qreal)> progressChanged;
    std::function<void()> finished;
    void selectDemo(int demo)
    {
        m_demo = qBound(0, demo, 3);
        replay();
    }
    void replay()
    {
        m_animation->stop();
        m_progress = 1;
        if (isVisible() && QApplication::isEffectEnabled(Qt::UI_AnimateMenu)) {
            m_progress = 0;
            m_animation->start();
        }
        if (progressChanged) progressChanged(m_progress);
        update();
    }
    int demo() const { return m_demo; }
    bool isAnimating() const { return m_animation->state() == QAbstractAnimation::Running; }
    QSize sizeHint() const override { return {640, 220}; }
    int heightForWidth(int width) const override { return qRound(width * 220.0 / 640.0); }
protected:
    void hideEvent(QHideEvent* event) override
    {
        m_animation->stop();
        QWidget::hideEvent(event);
    }
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
        const qreal scale = width() / 640.0;
        p.translate(0, qMax(0.0, (height() - 220 * scale) / 2));
        p.scale(scale, scale);
        auto panel = [&](QRectF rect, QColor color, qreal radius = 12) {
            p.setPen(Qt::NoPen); p.setBrush(color); p.drawRoundedRect(rect, radius, radius);
        };
        auto label = [&](QRectF rect, const QString& text, int size = 13, QColor color = QColor("#edf0f4")) {
            QFont font = this->font(); font.setPixelSize(size); font.setWeight(QFont::DemiBold);
            p.setFont(font); p.setPen(color); p.drawText(rect, Qt::AlignVCenter | Qt::AlignLeft, text);
        };
        auto button = [&](QRectF rect, const QString& text, bool primary = false) {
            panel(rect, primary ? QColor("#f2a365") : QColor("#383c43"), 9);
            QFont font = this->font(); font.setPixelSize(12); font.setWeight(QFont::DemiBold); p.setFont(font);
            p.setPen(primary ? QColor("#202124") : QColor("#edf0f4")); p.drawText(rect, Qt::AlignCenter, text);
        };
        auto image = [&](QRectF rect) {
            p.save(); QPainterPath path; path.addRoundedRect(rect, 10, 10); p.setClipPath(path);
            p.drawPixmap(rect.toRect(), QPixmap(":/pxl/cone-dawn-banner.png")); p.restore();
        };
        panel(QRectF(0, 0, 640, 220), QColor("#171b20"), 14);
        panel(QRectF(8, 8, 34, 204), QColor("#272c32"), 10);
        for (int row = 0; row < 4; ++row) panel(QRectF(19, 26 + row * 38, 12, 12), row == m_demo ? QColor("#f2a365") : QColor("#697481"), 4);
        const int stage = m_progress < .28 ? 0 : m_progress < .57 ? 1 : m_progress < .82 ? 2 : 3;
        QPointF cursor(500, 44);
        if (m_demo == 0) {
            label(QRectF(60, 14, 260, 30), welcomeText("Library"), 17);
            button(QRectF(455, 19, 164, 32), welcomeText("New instance"), true);
            if (stage == 1) {
                panel(QRectF(184, 49, 330, 155), QColor("#30353c"));
                label(QRectF(204, 55, 280, 26), welcomeText("Create an instance"), 16);
                const char* choices[] = {"From scratch", "Find a modpack", "Install from file", "Import from another launcher"};
                for (int row = 0; row < 4; ++row)
                    button(QRectF(204, 87 + row * 26, 288, 22), welcomeText(choices[row]), row == 0);
                cursor = QPointF(330, 97);
            } else if (stage == 2) {
                panel(QRectF(184, 57, 330, 148), QColor("#30353c"));
                label(QRectF(204, 65, 280, 26), welcomeText("Create an instance"), 16);
                button(QRectF(204, 103, 130, 28), "Minecraft 1.21.1");
                button(QRectF(344, 103, 145, 28), "Fabric", true);
                button(QRectF(344, 155, 145, 30), welcomeText("Create"), true);
                cursor = QPointF(410, 171);
            } else if (stage == 3) {
                panel(QRectF(60, 69, 220, 127), QColor("#2a3237"));
                label(QRectF(76, 83, 190, 26), welcomeText("My adventure"), 16);
                label(QRectF(76, 112, 190, 23), "Minecraft 1.21.1 · Fabric", 12, QColor("#aeb8c6"));
                button(QRectF(159, 151, 100, 30), welcomeText("Play"), true);
            }
        } else if (m_demo == 1) {
            label(QRectF(60, 14, 310, 30), welcomeText("Browse content"), 17);
            button(QRectF(60, 53, 550, 29), welcomeText("Search mods, resource packs and shaders"));
            for (int row = 0; row < 2; ++row) {
                const int y = 96 + row * 55;
                panel(QRectF(60, y, 550, 47), QColor("#2a3038"));
                panel(QRectF(72, y + 9, 28, 28), row ? QColor("#779e83") : QColor("#bba17d"), 7);
                label(QRectF(113, y + 5, 290, 35), row ? "Iris Shaders" : "Sodium", 15);
                button(QRectF(468, y + 9, 130, 29), welcomeText(stage >= 2 && !row ? "Installed" : "Add"), stage < 2 && !row);
            }
            cursor = QPointF(525, 118);
            if (stage == 1) {
                panel(QRectF(184, 60, 330, 138), QColor("#3b3c40"));
                label(QRectF(202, 75, 295, 28), welcomeText("Review selected content"), 16);
                label(QRectF(202, 107, 280, 25), "Sodium · Fabric", 14);
                button(QRectF(339, 151, 155, 30), welcomeText("Install selected"), true);
                cursor = QPointF(418, 167);
            }
        } else if (m_demo == 2) {
            image(QRectF(60, 18, 550, 102));
            button(QRectF(76, 76, 110, 30), welcomeText("Play"), true);
            if (stage == 0) {
                panel(QRectF(202, 40, 320, 136), QColor("#30353c"));
                label(QRectF(220, 48, 270, 28), welcomeText("Installing game files"), 16);
                panel(QRectF(220, 93, 280, 5), QColor("#51565f"), 2);
                panel(QRectF(220, 93, 150, 5), QColor("#f2a365"), 2);
                button(QRectF(385, 126, 115, 30), welcomeText("Hide")); cursor = QPointF(439, 141);
            } else {
                label(QRectF(60, 166, 320, 27), welcomeText("Downloads continue in the background"), 13);
                panel(QRectF(60, 204, 550, 4), QColor("#383e47"), 2);
                panel(QRectF(60, 204, 550 * qBound(.2, m_progress, 1.0), 4), QColor("#f2a365"), 2);
                cursor = QPointF(25, 146);
                if (stage >= 2) {
                    panel(QRectF(49, 65, 325, 97), QColor("#30363e"));
                    label(QRectF(65, 73, 290, 25), welcomeText("Downloads & tasks"), 15);
                    label(QRectF(65, 111, 290, 28), welcomeText(stage == 3 ? "Completed" : "Installing game files"), 13);
                }
            }
        } else {
            label(QRectF(60, 14, 320, 30), welcomeText("Screenshots"), 17);
            image(QRectF(60, 62, 164, 105)); image(QRectF(240, 62, 164, 105));
            cursor = QPointF(138, 109);
            if (stage == 1) {
                const qreal zoom = QApplication::isEffectEnabled(Qt::UI_AnimateMenu) ? qBound(0.0, (m_progress - .28) / .18, 1.0) : 1.0;
                const QRectF enlarged(60 + 10 * zoom, 62 - 12 * zoom, 164 + 340 * zoom, 105 + 44 * zoom);
                image(enlarged);
                const QRectF close(enlarged.right() - 34, enlarged.top() + 8, 26, 26);
                panel(close, QColor(24, 28, 33, 220), 13);
                p.setPen(QPen(QColor("#edf0f4"), 2, Qt::SolidLine, Qt::RoundCap));
                const auto center = close.center();
                p.drawLine(center + QPointF(-4, -4), center + QPointF(4, 4));
                p.drawLine(center + QPointF(-4, 4), center + QPointF(4, -4));
                cursor = close.center();
            } else if (stage >= 2) {
                panel(QRectF(171, 71, 435, 110), QColor("#343a42"));
                button(QRectF(179, 79, 419, 28), welcomeText("Make banner for this instance"), true);
                label(QRectF(192, 110, 400, 28), welcomeText("Make banner for another instance…"), 12);
                label(QRectF(192, 142, 390, 28), welcomeText("Copy image"), 12);
                cursor = QPointF(385, 93);
                if (stage == 3) {
                    panel(QRectF(184, 62, 330, 138), QColor("#30353c"));
                    label(QRectF(204, 72, 290, 28), welcomeText("Set instance banner"), 16);
                    label(QRectF(204, 108, 290, 25), welcomeText("My adventure"), 14);
                    button(QRectF(339, 155, 155, 30), welcomeText("Use banner"), true);
                    cursor = QPointF(418, 171);
                }
            }
        }
        if (m_progress < .97 && QApplication::isEffectEnabled(Qt::UI_AnimateMenu)) {
            const QPointF targets[4][4] = {
                {{537, 35}, {330, 97}, {410, 171}, {210, 166}},
                {{525, 118}, {418, 167}, {525, 118}, {525, 118}},
                {{439, 141}, {25, 146}, {25, 146}, {25, 146}},
                {{138, 109}, {548, 70}, {385, 93}, {418, 171}}};
            const qreal boundaries[] = {0, .28, .57, .82, 1};
            const QPointF previous = stage ? targets[m_demo][stage - 1] : QPointF(596, 195);
            const qreal fraction = qBound(0.0, (m_progress - boundaries[stage]) / ((boundaries[stage + 1] - boundaries[stage]) * .65), 1.0);
            cursor = previous + (targets[m_demo][stage] - previous) * QEasingCurve(QEasingCurve::InOutCubic).valueForProgress(fraction);
            const qreal pulse = qMax(0.0, 1.0 - qAbs(std::fmod(m_progress * 3, 1.0) - .75) * 10);
            p.setPen(QPen(QColor(242, 163, 101, int(pulse * 140)), 2)); p.setBrush(Qt::NoBrush);
            p.drawEllipse(cursor, 9 + pulse * 7, 9 + pulse * 7);
            QPainterPath pointer; pointer.moveTo(cursor); pointer.lineTo(cursor + QPointF(3, 17));
            pointer.lineTo(cursor + QPointF(7, 11)); pointer.lineTo(cursor + QPointF(14, 10)); pointer.closeSubpath();
            p.setPen(QPen(QColor("#171b20"), 1)); p.setBrush(QColor("#f1f3f5")); p.drawPath(pointer);
        }
    }
private:
    QVariantAnimation* m_animation = nullptr;
    int m_demo = 0;
    qreal m_progress = 1;
};
} // namespace Pxl

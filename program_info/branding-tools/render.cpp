// SPDX-License-Identifier: GPL-3.0-only
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QSvgRenderer>
#include <QTextStream>

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    if (app.arguments().size() != 3) {
        QTextStream(stderr) << "Usage: pxl-branding-render source.svg output.png\n";
        return 2;
    }
    QSvgRenderer renderer(app.arguments().at(1));
    if (!renderer.isValid() || renderer.viewBoxF().isEmpty()) return 3;
    const QSizeF size = renderer.viewBoxF().size().scaled(QSizeF(896,896),Qt::KeepAspectRatio);
    QImage image(1024,1024,QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    renderer.render(&painter,QRectF(QPointF((1024-size.width())/2,(1024-size.height())/2),size));
    painter.end();
    return image.save(app.arguments().at(2),"PNG") ? 0 : 4;
}

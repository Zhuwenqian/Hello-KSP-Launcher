#include "iconutils.h"
#include <QFile>
#include <QSvgRenderer>
#include <QPainter>
#include <QPixmap>
#include <QByteArray>

QIcon IconUtils::tintedIcon(const QString &svgPath, const QString &color)
{
    QFile file(svgPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QIcon(svgPath);
    }

    QByteArray data = file.readAll();
    file.close();

    // 替换currentColor为指定颜色
    data.replace("currentColor", color.toUtf8());

    QSvgRenderer renderer(data);
    if (!renderer.isValid()) {
        return QIcon(svgPath);
    }

    QPixmap pixmap(renderer.defaultSize());
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    renderer.render(&painter);
    painter.end();

    return QIcon(pixmap);
}

QPixmap IconUtils::tintedPixmap(const QString &svgPath, const QString &color, const QSize &size)
{
    QFile file(svgPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QPixmap();
    }

    QByteArray data = file.readAll();
    file.close();

    data.replace("currentColor", color.toUtf8());

    QSvgRenderer renderer(data);
    if (!renderer.isValid()) {
        return QPixmap();
    }

    // 按目标尺寸直接栅格化：大图（如 260x260 缩略图框）不会因固定 24px 源放大而模糊
    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    renderer.render(&painter);
    painter.end();
    return pixmap;
}

QString IconUtils::iconColorForTheme(const QString &theme)
{
    if (theme == "dark") {
        return "#ffffff";
    }
    return "#1e1e1e";
}

#ifndef ICONUTILS_H
#define ICONUTILS_H

#include <QIcon>
#include <QPixmap>
#include <QString>
#include <QSize>

class IconUtils
{
public:
    // 根据颜色渲染SVG图标，替换currentColor
    static QIcon tintedIcon(const QString& svgPath, const QString& color = "#ffffff");

    // 按指定像素尺寸渲染着色后的 SVG 位图（用于大图/适配展示框，避免放大发糊）
    static QPixmap tintedPixmap(const QString& svgPath, const QString& color, const QSize& size);

    // 根据主题获取图标颜色
    static QString iconColorForTheme(const QString& theme);
};

#endif // ICONUTILS_H

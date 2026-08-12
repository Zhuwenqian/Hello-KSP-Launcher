#ifndef ICONUTILS_H
#define ICONUTILS_H

#include <QIcon>
#include <QString>

class IconUtils
{
public:
    // 根据颜色渲染SVG图标，替换currentColor
    static QIcon tintedIcon(const QString& svgPath, const QString& color = "#ffffff");

    // 根据主题获取图标颜色
    static QString iconColorForTheme(const QString& theme);
};

#endif // ICONUTILS_H

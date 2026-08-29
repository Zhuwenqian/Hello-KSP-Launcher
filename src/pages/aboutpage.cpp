#include "aboutpage.h"
#include "../appversion.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QDesktopServices>
#include <QUrl>
#include <QFrame>

AboutPage::AboutPage(QWidget *parent)
    : QWidget(parent), m_scrollArea(nullptr)
{
    // 外层滚动区域
    QVBoxLayout* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    rootLayout->addWidget(m_scrollArea);

    // 外层内容容器保留半透明背板；板块本身不设背景，条目以卡片按钮渲染
    QWidget* container = new QWidget(m_scrollArea);
    container->setObjectName("settingsContent");
    m_scrollArea->setWidget(container);

    QVBoxLayout* mainLayout = new QVBoxLayout(container);
    mainLayout->setContentsMargins(20, 10, 20, 20);
    mainLayout->setSpacing(12);

    QLabel* titleLabel = new QLabel(tr("关于"), container);
    titleLabel->setObjectName("pageTitle");
    mainLayout->addWidget(titleLabel);

    QPixmap appIcon = QIcon(":/appicon.ico").pixmap(256, 256);

    // 作者头像：圆形裁剪
    QPixmap avatar;
    {
        QPixmap author(":/author.png");
        if (!author.isNull()) {
            const int s = 72;
            author = author.scaled(s, s, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            QPixmap round(s, s);
            round.fill(Qt::transparent);
            QPainter p(&round);
            p.setRenderHint(QPainter::Antialiasing);
            QPainterPath clip;
            clip.addEllipse(0, 0, s, s);
            p.setClipPath(clip);
            p.drawPixmap(author.rect().center() - QPoint(s / 2, s / 2), author);
            p.end();
            avatar = round;
        }
    }

    // ---------------- 关于 ----------------
    mainLayout->addWidget(makeSectionTitle(tr("关于"), container));

    mainLayout->addWidget(makeLinkRow(
        tr("Hello KSP Launcher"),
        tr("v%1").arg(QStringLiteral(HKSPL_APP_VERSION)),
        QString(),
        appIcon));

    mainLayout->addWidget(makeLinkRow(
        tr("Zhu Wenqian"),
        tr("bilibili @ZhuWenqian-KSP"),
        QStringLiteral("https://space.bilibili.com/1299073087?"),
        avatar));

    // ---------------- 鸣谢 ----------------
    mainLayout->addWidget(makeSectionTitle(tr("鸣谢"), container));

    mainLayout->addWidget(makeLinkRow(
        tr("KSP-CKAN 团队"),
        tr("提供模组管理 C# 参考代码和仓库索引"),
        QStringLiteral("https://github.com/KSP-CKAN")));

    mainLayout->addWidget(makeLinkRow(
        tr("Hello Minecraft Launcher 项目"),
        tr("提供许多 UI 设计上的参考"),
        QStringLiteral("https://github.com/HMCL-dev/HMCL")));

    mainLayout->addWidget(makeLinkRow(
        tr("gh-proxy.com"),
        tr("提供 CKAN 索引和模组下载加速"),
        QStringLiteral("https://gh-proxy.com/")));

    mainLayout->addWidget(makeLinkRow(
        tr("ghfast.top"),
        tr("提供 CKAN 索引和模组下载加速"),
        QStringLiteral("https://ghfast.top/")));

    // ---------------- 依赖 ----------------
    mainLayout->addWidget(makeSectionTitle(tr("依赖"), container));

    mainLayout->addWidget(makeLinkRow(
        tr("Qt 6"),
        tr("Copyright © The Qt Company Ltd · Licence under LGPL v3"),
        QStringLiteral("https://www.qt.io/development/qt-framework/qt6")));

    mainLayout->addWidget(makeLinkRow(
        tr("miniz"),
        tr("Copyright © richgel999 · License: MIT"),
        QStringLiteral("https://github.com/richgel999/miniz")));

    mainLayout->addWidget(makeLinkRow(
        tr("libckan"),
        tr("Copyright © Zhu Wenqian · License: GPL v3"),
        QStringLiteral("https://github.com/Zhuwenqian/libcakn-cpp")));

    // ---------------- 法律声明 ----------------
    mainLayout->addWidget(makeSectionTitle(tr("法律声明"), container));

    mainLayout->addWidget(makeLinkRow(tr("版权所有 © 2026 Zhu Wenqian")));

    mainLayout->addWidget(makeLinkRow(
        tr("许可证 GPL v3"),
        QString(),
        QStringLiteral("https://github.com/Zhuwenqian/Hello-KSP-Launcher/blob/main/LICENSE")));

    mainLayout->addStretch();
}

// 板块小标题
QLabel* AboutPage::makeSectionTitle(const QString &text, QWidget *parent)
{
    QLabel* label = new QLabel(text, parent);
    label->setObjectName("aboutSectionTitle");
    return label;
}

// 生成一张卡片按钮：可选前置图标 + 标题 + 描述；整卡可点击打开 url
QPushButton* AboutPage::makeLinkRow(const QString &title, const QString &desc,
                                    const QString &url, const QPixmap &icon)
{
    QPushButton* btn = new QPushButton(this);
    btn->setObjectName("aboutCard");
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFocusPolicy(Qt::NoFocus);

    QHBoxLayout* row = new QHBoxLayout(btn);
    row->setContentsMargins(16, 14, 16, 14);
    row->setSpacing(14);

    if (!icon.isNull()) {
        QLabel* iconLabel = new QLabel(btn);
        iconLabel->setPixmap(icon.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        iconLabel->setFixedSize(48, 48);
        row->addWidget(iconLabel, 0, Qt::AlignVCenter);
    }

    QVBoxLayout* col = new QVBoxLayout();
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(3);

    QLabel* titleLabel = new QLabel(title, btn);
    titleLabel->setObjectName("aboutCardTitle");
    titleLabel->setWordWrap(true);
    col->addWidget(titleLabel);

    if (!desc.isEmpty()) {
        QLabel* descLabel = new QLabel(desc, btn);
        descLabel->setObjectName("aboutCardDesc");
        descLabel->setWordWrap(true);
        col->addWidget(descLabel);
    }

    row->addLayout(col, 1);

    if (!url.isEmpty()) {
        btn->setToolTip(url);
        const QString target = url;
        connect(btn, &QPushButton::clicked, this, [this, target]() {
            openUrl(target);
        });
    }
    return btn;
}

void AboutPage::openUrl(const QString &url)
{
    QDesktopServices::openUrl(QUrl(url));
}
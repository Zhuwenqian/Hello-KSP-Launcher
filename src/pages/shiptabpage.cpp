#include "shiptabpage.h"
#include "../iconutils.h"
#include "../instancemanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QStackedWidget>
#include <QLabel>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QMessageBox>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QPixmap>
#include <QEvent>
#include <QMimeData>
#include <QUrl>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QtConcurrent/QtConcurrentRun>

namespace {
// 类型下标 → Ships/VAB|SPH 目录名
const char* kShipTypeNames[2] = {"VAB", "SPH"};
}

ShipTabPage::ShipTabPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("shipsTabPage");
    setAttribute(Qt::WA_StyledBackground, true);
    setupUI();
}

void ShipTabPage::setupUI()
{
    QVBoxLayout* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_stack = new QStackedWidget(this);

    // ---- 列表页 ----
    QWidget* listPage = new QWidget(m_stack);
    listPage->setObjectName("shipListPanel");
    listPage->setAttribute(Qt::WA_StyledBackground, true);
    QVBoxLayout* listLayout = new QVBoxLayout(listPage);
    listLayout->setContentsMargins(15, 10, 15, 15);
    listLayout->setSpacing(10);

    // 顶栏：右侧「导入飞船」按钮
    QHBoxLayout* listHeader = new QHBoxLayout();
    m_importBtn = new QPushButton(IconUtils::tintedIcon(":/icons/download.svg", "#ffffff"),
                                  tr("  导入飞船"), listPage);
    m_importBtn->setObjectName("primaryButton");
    m_importBtn->setFixedHeight(36);
    connect(m_importBtn, &QPushButton::clicked, this, &ShipTabPage::onImportShipsClicked);
    listHeader->addStretch();
    listHeader->addWidget(m_importBtn);
    listLayout->addLayout(listHeader);

    m_typeTabs = new QTabWidget(listPage);
    m_typeTabs->setObjectName("shipTypeTabs");
    for (int i = 0; i < 2; ++i) {
        m_lists[i] = new QListWidget(m_typeTabs);
        m_lists[i]->setObjectName("shipsListWidget");
        // 允许把 .craft 文件拖到列表导入
        m_lists[i]->setAcceptDrops(true);
        m_lists[i]->installEventFilter(this);
        // 点击行任一处（除右侧删除按钮）进入该飞船详情
        connect(m_lists[i], &QListWidget::itemClicked, this, &ShipTabPage::onShipItemClicked);
        m_typeTabs->addTab(m_lists[i], tr(kShipTypeNames[i]));
    }
    m_typeTabs->setCurrentIndex(0); // 默认 VAB
    connect(m_typeTabs, &QTabWidget::currentChanged, this, &ShipTabPage::onTabChanged);

    listLayout->addWidget(m_typeTabs, 1);

    // ---- 详情页 ----
    QWidget* detailPage = new QWidget(m_stack);
    detailPage->setObjectName("shipDetailPanel");
    detailPage->setAttribute(Qt::WA_StyledBackground, true);
    QVBoxLayout* detailLayout = new QVBoxLayout(detailPage);
    detailLayout->setContentsMargins(15, 10, 15, 15);
    detailLayout->setSpacing(12);

    QHBoxLayout* topRow = new QHBoxLayout();
    m_backButton = new QPushButton(IconUtils::tintedIcon(":/icons/back.svg", "#ffffff"), tr(" 返回"), detailPage);
    m_backButton->setObjectName("backButton");
    m_backButton->setFixedHeight(40);
    m_backButton->setMinimumWidth(100);
    connect(m_backButton, &QPushButton::clicked, this, &ShipTabPage::onBackToListClicked);
    QLabel* detailTitle = new QLabel(tr("飞船详情"), detailPage);
    detailTitle->setObjectName("pageTitle");
    topRow->addWidget(m_backButton);
    topRow->addWidget(detailTitle, 1);
    detailLayout->addLayout(topRow);

    QHBoxLayout* body = new QHBoxLayout();
    body->setSpacing(20);

    QVBoxLayout* infoLayout = new QVBoxLayout();
    infoLayout->setSpacing(10);

    m_detailName = new QLabel(detailPage);
    m_detailName->setStyleSheet("font-size: 18pt; font-weight: bold;");

    m_detailVersion = new QLabel(detailPage);
    m_detailVersion->setStyleSheet("color: #888; font-size: 10pt;");

    m_detailPartCount = new QLabel(detailPage);
    m_detailPartCount->setStyleSheet("color: #888; font-size: 10pt;");

    QLabel* descLabel = new QLabel(tr("描述"), detailPage);
    descLabel->setStyleSheet("font-size: 10pt; color: #666; margin-top: 8px;");

    m_detailDescription = new QPlainTextEdit(detailPage);
    m_detailDescription->setObjectName("shipDetailDescription");
    m_detailDescription->setReadOnly(true); // 描述只读
    m_detailDescription->setPlaceholderText(tr("（无描述）"));
    m_detailDescription->setMinimumHeight(140);

    infoLayout->addWidget(m_detailName);
    infoLayout->addWidget(m_detailVersion);
    infoLayout->addWidget(m_detailPartCount);
    infoLayout->addWidget(descLabel);
    infoLayout->addWidget(m_detailDescription, 1);

    QVBoxLayout* thumbColumn = new QVBoxLayout();
    m_detailThumb = new QLabel(detailPage);
    m_detailThumb->setObjectName("shipThumb");
    m_detailThumb->setAlignment(Qt::AlignCenter);
    m_detailThumb->setScaledContents(true);
    m_detailThumb->setFixedSize(260, 260); // 正方形图片框
    m_detailThumb->setStyleSheet("border: 1px solid rgba(128,128,128,0.5); border-radius: 6px; background: #00000022;");
    thumbColumn->addWidget(m_detailThumb);
    thumbColumn->addStretch();

    body->addLayout(infoLayout, 1);
    body->addLayout(thumbColumn); // 右侧放图

    detailLayout->addLayout(body, 1);

    m_stack->addWidget(listPage);
    m_stack->addWidget(detailPage);
    m_stack->setCurrentIndex(0);

    rootLayout->addWidget(m_stack);
}

void ShipTabPage::setInstanceId(const QString& id)
{
    // 实例模式：飞船根 = 实例根目录（getShipsDir 会拼出 <根>/Ships）
    setShipsBase(ConfigManager::instance().getInstance(id).path);
}

void ShipTabPage::setShipsBase(const QString& basePath)
{
    m_shipsBasePath = basePath;
    // 不在此处加载：进入飞船 tab 时由 InstanceDetailPage::showSection 调用 loadShips()（异步）。
}

void ShipTabPage::loadShips()
{
    if (m_shipsBasePath.isEmpty()) {
        for (int i = 0; i < 2; ++i) {
            m_lists[i]->clear();
        }
        m_stack->setCurrentIndex(0);
        return;
    }
    for (int i = 0; i < 2; ++i) {
        m_lists[i]->clear();
        m_lists[i]->addItem(tr("正在加载飞船..."));
    }
    // 重新进入飞船管理时回列表页（若上次停在详情）
    m_stack->setCurrentIndex(0);

    // 后台线程：遍历 VAB/SPH 目录 + 逐文件解析 .craft（纯文件读取，线程安全）。
    // 完成后回主线程填充两个列表，避免大量飞船下进入页面卡顿。
    const QString base = m_shipsBasePath;
    auto future = QtConcurrent::run([base]() {
        QVector<ShipListEntry> out;
        for (int i = 0; i < 2; ++i) {
            const QString type = QString::fromLatin1(kShipTypeNames[i]);
            const QStringList craftFiles = InstanceManager::instance().listCraftFiles(base, type);
            const QString typeDir = QDir(InstanceManager::instance().getShipsDir(base)).filePath(type);
            out.reserve(out.size() + craftFiles.size());
            for (const QString& craftName : craftFiles) {
                ShipListEntry e;
                e.type = type;
                e.craftPath = QDir(typeDir).filePath(craftName);
                e.info = InstanceManager::instance().loadCraftInfo(e.craftPath);
                out.append(e);
            }
        }
        return out;
    });
    if (!m_shipsLoadWatcher) {
        m_shipsLoadWatcher = new QFutureWatcher<QVector<ShipListEntry>>(this);
        connect(m_shipsLoadWatcher, &QFutureWatcher<QVector<ShipListEntry>>::finished,
                this, &ShipTabPage::onShipsLoadFinished);
    }
    m_shipsLoadWatcher->setFuture(future);
}

void ShipTabPage::onShipsLoadFinished()
{
    for (int i = 0; i < 2; ++i)
        m_lists[i]->clear();

    const QVector<ShipListEntry> entries = m_shipsLoadWatcher->result();
    bool any[2] = { false, false };
    for (const ShipListEntry& e : entries) {
        const int ti = (e.type == QLatin1String("SPH")) ? 1 : 0;
        addShipRow(m_lists[ti], e.craftPath, e.type, e.info);
        any[ti] = true;
    }
    for (int i = 0; i < 2; ++i) {
        if (!any[i])
            m_lists[i]->addItem(tr("（未检测到飞船）"));
    }
}

void ShipTabPage::addShipRow(QListWidget* list, const QString& craftPath, const QString& type,
                             const ShipInfo& info)
{
    QWidget* itemWidget = new QWidget(list);
    QHBoxLayout* rowLayout = new QHBoxLayout(itemWidget);
    rowLayout->setContentsMargins(20, 15, 20, 15);

    QVBoxLayout* textLayout = new QVBoxLayout();
    textLayout->setSpacing(4);

    QLabel* nameLabel = new QLabel(info.name, itemWidget); // 无 .craft 后缀
    nameLabel->setStyleSheet("font-weight: bold; font-size: 11pt;");

    QString versionText = info.version;
    if (versionText.isEmpty()) {
        versionText = tr("未知");
    }
    QLabel* infoLabel = new QLabel(tr("游戏版本: %1 · 部件数: %2").arg(versionText).arg(info.partCount), itemWidget);
    infoLabel->setStyleSheet("color: #888; font-size: 9pt;");

    textLayout->addWidget(nameLabel);
    textLayout->addWidget(infoLabel);

    // 每行最右侧删除按钮：确认后把 craft 移到回收站
    QPushButton* deleteBtn = new QPushButton(IconUtils::tintedIcon(":/icons/trash-2.svg", "#888"), "", itemWidget);
    deleteBtn->setObjectName("iconButton");
    deleteBtn->setFixedSize(36, 36);
    deleteBtn->setCursor(Qt::PointingHandCursor);
    deleteBtn->setToolTip(tr("删除飞船"));
    connect(deleteBtn, &QPushButton::clicked, this,
            [this, craftPath, type]() { onDeleteShipClicked(craftPath, type); });

    rowLayout->addLayout(textLayout, 1);
    rowLayout->addWidget(deleteBtn);

    QListWidgetItem* item = new QListWidgetItem(list);
    item->setSizeHint(QSize(0, 78));
    item->setData(Qt::UserRole, craftPath);
    item->setData(Qt::UserRole + 1, type);
    list->addItem(item);
    list->setItemWidget(item, itemWidget);
}

void ShipTabPage::onTabChanged(int index)
{
    // 数据在 loadShips 时已全部填充；切换 tab 仅需保证当前类型下的列表可见
    Q_UNUSED(index);
}

void ShipTabPage::onShipItemClicked(QListWidgetItem* item)
{
    const QString craftPath = item->data(Qt::UserRole).toString();
    const int typeIndex = (item->data(Qt::UserRole + 1).toString() == "SPH") ? 1 : 0;
    if (craftPath.isEmpty()) {
        return;
    }
    showDetail(craftPath, typeIndex);
}

void ShipTabPage::showDetail(const QString& path, int typeIndex)
{
    loadDetail(path, typeIndex);
    m_stack->setCurrentIndex(1);
}

void ShipTabPage::loadDetail(const QString& path, int typeIndex)
{
    ShipInfo info = InstanceManager::instance().loadCraftInfo(path);
    m_detailName->setText(info.name);
    m_detailVersion->setText(tr("游戏版本: %1").arg(info.version.isEmpty() ? tr("未知") : info.version));
    m_detailPartCount->setText(tr("部件数: %1").arg(info.partCount));
    m_detailDescription->setPlainText(info.description);

    // 缩略图：Ships/@thumbs/{type}/{名字}.png|.jpg；缺失时用火箭 SVG 兜底
    const QString thumb = InstanceManager::instance().getShipThumbPath(
        m_shipsBasePath, QString::fromLatin1(kShipTypeNames[typeIndex]), info.fileName);
    if (!thumb.isEmpty()) {
        QPixmap pm(thumb);
        if (!pm.isNull()) {
            m_detailThumb->setPixmap(pm.scaled(m_detailThumb->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            // 火箭兜底：按缩略图框实际尺寸渲染，避免 96px 位图拉伸到 260 框发糊
            const QSize box = m_detailThumb->size();
            m_detailThumb->setPixmap(IconUtils::tintedPixmap(":/icons/rocket.svg", "#ffffff", box));
        }
    } else {
        const QSize box = m_detailThumb->size();
        m_detailThumb->setPixmap(IconUtils::tintedPixmap(":/icons/rocket.svg", "#ffffff", box));
    }
}

void ShipTabPage::onBackToListClicked()
{
    m_stack->setCurrentIndex(0);
}

void ShipTabPage::onDeleteShipClicked(const QString& craftPath, const QString& type)
{
    QFileInfo fi(craftPath);
    if (!fi.exists()) {
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(this, tr("删除飞船"),
        tr("确定要删除飞船 '%1' 吗？\n该文件将被移动到回收站。").arg(fi.completeBaseName()),
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) {
        return;
    }

    if (InstanceManager::instance().moveCraftToTrash(craftPath)) {
        // 静默异步刷新两个类型列表
        loadShips();
    } else {
        QMessageBox::warning(this, tr("删除失败"), tr("无法删除飞船，请检查文件是否被占用。"));
    }
}

void ShipTabPage::onImportShipsClicked()
{
    // 单选/多选 .craft 文件；导入到当前 tab 对应的类型目录
    const QStringList files = QFileDialog::getOpenFileNames(
        this, tr("导入飞船"), QString(), tr("飞船文件 (*.craft)"));
    if (files.isEmpty()) {
        return;
    }
    importShipFiles(files, m_typeTabs->currentIndex());
}

void ShipTabPage::importShipFiles(const QStringList& paths, int typeIndex)
{
    if (m_shipsBasePath.isEmpty()) {
        return;
    }

    const QString type = QString::fromLatin1(kShipTypeNames[typeIndex]);
    const QString dirPath = QDir(InstanceManager::instance().getShipsDir(m_shipsBasePath)).filePath(type);
    const QDir dir(dirPath);
    if (!dir.exists()) {
        return;
    }

    // 目标目录内已存在的文件名（小写形式），用于大小写不敏感的重名检测
    const QStringList existingNames = dir.entryList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);

    // 禁止从本飞船根目录自身的 Ships/VAB、Ships/SPH 目录导入
    const QString shipsDir = InstanceManager::instance().getShipsDir(m_shipsBasePath);
    auto absOf = [](const QString& p) {
        return QDir::cleanPath(QFileInfo(p).absoluteFilePath());
    };
    const QString forbiddenDir[2] = {
        absOf(QDir(shipsDir).filePath(QStringLiteral("VAB"))),
        absOf(QDir(shipsDir).filePath(QStringLiteral("SPH")))
    };

    bool imported = false;
    for (const QString& srcPath : paths) {
        QFileInfo srcInfo(srcPath);
        if (!srcInfo.exists()) {
            continue;
        }
        const QString srcName = srcInfo.fileName();
        if (!srcName.endsWith(QStringLiteral(".craft"), Qt::CaseInsensitive)) {
            continue;
        }
        // 源文件位于禁止导入目录（本实例 VAB/SPH）则跳过
        const QString srcDir = absOf(srcInfo.absolutePath());
        if (srcDir.compare(forbiddenDir[0], Qt::CaseInsensitive) == 0
            || srcDir.compare(forbiddenDir[1], Qt::CaseInsensitive) == 0) {
            continue;
        }

        // 重名检测（大小写不敏感），命中则询问是否覆盖
        QString existingPath;
        for (const QString& en : existingNames) {
            if (en.compare(srcName, Qt::CaseInsensitive) == 0) {
                existingPath = dir.filePath(en);
                break;
            }
        }

        if (!existingPath.isEmpty()) {
            QMessageBox::StandardButton reply = QMessageBox::question(
                this, tr("覆盖飞船"),
                tr("飞船 \"%1\" 已存在，是否覆盖？").arg(srcInfo.completeBaseName()),
                QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
            if (reply == QMessageBox::Cancel) {
                break; // 取消：中止剩余文件
            }
            if (reply == QMessageBox::No) {
                continue; // 否：跳过当前文件
            }
            // 覆盖：删除现有文件（保留其在目标目录的大小写），随后复制
            QFile::remove(existingPath);
        }

        QString target = existingPath.isEmpty() ? dir.filePath(srcName) : existingPath;
        if (QFile::copy(srcPath, target)) {
            imported = true;
        } else {
            QMessageBox::warning(this, tr("导入失败"),
                                 tr("无法导入 \"%1\"，请检查文件是否可读或磁盘空间。").arg(srcName));
        }
    }

    if (imported) {
        loadShips();
    }
}

bool ShipTabPage::eventFilter(QObject* obj, QEvent* event)
{
    int typeIndex = -1;
    if (obj == m_lists[0]) {
        typeIndex = 0;
    } else if (obj == m_lists[1]) {
        typeIndex = 1;
    }

    if (typeIndex >= 0) {
        if (event->type() == QEvent::DragEnter) {
            auto* de = static_cast<QDragEnterEvent*>(event);
            if (de->mimeData()->hasUrls() && de->mimeData()->urls().first().isLocalFile()) {
                de->acceptProposedAction();
                return true;
            }
            return false;
        }
        if (event->type() == QEvent::Drop) {
            auto* de = static_cast<QDropEvent*>(event);
            QStringList craftFiles;
            for (const QUrl& url : de->mimeData()->urls()) {
                if (url.isLocalFile()) {
                    const QString p = url.toLocalFile();
                    if (p.endsWith(QStringLiteral(".craft"), Qt::CaseInsensitive)) {
                        craftFiles.append(p);
                    }
                }
            }
            if (!craftFiles.isEmpty()) {
                importShipFiles(craftFiles, typeIndex);
            }
            de->acceptProposedAction();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void ShipTabPage::refreshIcons(const QString &color)
{
    m_backButton->setIcon(IconUtils::tintedIcon(":/icons/back.svg", color));
    m_importBtn->setIcon(IconUtils::tintedIcon(":/icons/download.svg", color));
}
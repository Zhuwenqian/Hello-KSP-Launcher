#include "savestabpage.h"
#include "../iconutils.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDir>
#include <QMessageBox>
#include <QtConcurrent/QtConcurrentRun>
#include "../instancemanager.h"

SavesTabPage::SavesTabPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("savesTabPage");
    setAttribute(Qt::WA_StyledBackground, true);
    setupUI();
}

void SavesTabPage::setupUI()
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(15, 10, 15, 15);
    layout->setSpacing(10);

    m_savesList = new QListWidget(this);
    m_savesList->setObjectName("savesListWidget");
    connect(m_savesList, &QListWidget::itemDoubleClicked, this, &SavesTabPage::onSaveItemDoubleClicked);
    layout->addWidget(m_savesList, 1);
}

void SavesTabPage::setInstanceId(const QString& id)
{
    m_instanceId = id;
    m_instance = ConfigManager::instance().getInstance(id);
    // 不在此处加载：进入存档 tab 时由 InstanceDetailPage::showSection 调用 loadSaves()（异步）。
}

void SavesTabPage::loadSaves()
{
    m_savesList->clear();
    if (m_instance.path.isEmpty()) {
        m_savesList->addItem(tr("（未检测到存档）"));
        return;
    }
    m_savesList->addItem(tr("正在加载存档..."));

    // 后台线程：遍历存档目录 + 逐文件解析 persistent.sfs（纯文件读取，线程安全）。
    // 完成后回主线程填充列表，避免大存档下进入页面卡顿。
    const QString gamePath = m_instance.path;
    auto future = QtConcurrent::run([gamePath]() {
        QVector<QPair<QString, SaveInfo>> out;
        const QStringList saveNames = InstanceManager::instance().listSaves(gamePath);
        const QString savesDir = InstanceManager::instance().getSavesDir(gamePath);
        out.reserve(saveNames.size());
        for (const QString& saveName : saveNames) {
            const QString savePath = QDir(savesDir).filePath(saveName);
            out.append(qMakePair(savePath, InstanceManager::instance().loadSaveInfo(savePath)));
        }
        return out;
    });
    if (!m_savesLoadWatcher) {
        m_savesLoadWatcher = new QFutureWatcher<QVector<QPair<QString, SaveInfo>>>(this);
        connect(m_savesLoadWatcher, &QFutureWatcher<QVector<QPair<QString, SaveInfo>>>::finished,
                this, &SavesTabPage::onSavesLoadFinished);
    }
    m_savesLoadWatcher->setFuture(future);
}

void SavesTabPage::onSavesLoadFinished()
{
    m_savesList->clear();
    const QVector<QPair<QString, SaveInfo>> items = m_savesLoadWatcher->result();

    if (items.isEmpty()) {
        m_savesList->addItem(tr("（未检测到存档）"));
        return;
    }

    for (const QPair<QString, SaveInfo>& entry : items) {
        const QString savePath = entry.first;
        const SaveInfo& info = entry.second;
        const QString saveName = QFileInfo(savePath).fileName();

        QWidget* itemWidget = new QWidget(m_savesList);
        QHBoxLayout* layout = new QHBoxLayout(itemWidget);
        layout->setContentsMargins(20, 15, 20, 15);

        QVBoxLayout* textLayout = new QVBoxLayout();
        textLayout->setSpacing(4);

        QLabel* nameLabel = new QLabel(saveName, itemWidget);
        nameLabel->setStyleSheet("font-weight: bold; font-size: 11pt;");

        QLabel* infoLabel = new QLabel(itemWidget);
        QString modeDisplay = info.mode;
        if (modeDisplay == "SANDBOX") modeDisplay = tr("沙盒模式");
        else if (modeDisplay == "CAREER") modeDisplay = tr("生涯模式");
        else if (modeDisplay == "SCIENCE_SANDBOX") modeDisplay = tr("科学模式");
        QString infoText = tr("版本: %1 | 模式: %2").arg(info.version, modeDisplay);
        if (info.modded) {
            infoText += tr(" | 模组");
        }
        infoLabel->setText(infoText);
        infoLabel->setStyleSheet("color: #888; font-size: 9pt;");

        textLayout->addWidget(nameLabel);
        textLayout->addWidget(infoLabel);

        // 最右侧为删除按钮：点击后提示用户存档将被移动到回收站（Windows），确认后删除。
        QPushButton* deleteBtn = new QPushButton(IconUtils::tintedIcon(":/icons/trash-2.svg", "#888"), "", itemWidget);
        deleteBtn->setObjectName("iconButton");
        deleteBtn->setFixedSize(36, 36);
        deleteBtn->setCursor(Qt::PointingHandCursor);
        deleteBtn->setToolTip(tr("删除存档"));
        connect(deleteBtn, &QPushButton::clicked, this, [this, savePath]() {
            onDeleteSaveClicked(savePath);
        });

        layout->addLayout(textLayout, 1);
        layout->addWidget(deleteBtn);

        QListWidgetItem* item = new QListWidgetItem(m_savesList);
        item->setSizeHint(QSize(0, 80));
        item->setData(Qt::UserRole, savePath);
        m_savesList->addItem(item);
        m_savesList->setItemWidget(item, itemWidget);
    }
}

void SavesTabPage::onSaveItemDoubleClicked(QListWidgetItem *item)
{
    QString savePath = item->data(Qt::UserRole).toString();
    if (!savePath.isEmpty()) {
        emit saveSelected(savePath, m_instance.name, m_instance.id);
    }
}

void SavesTabPage::onDeleteSaveClicked(const QString &saveFolderPath)
{
    QFileInfo fi(saveFolderPath);
    if (!fi.exists()) {
        return;
    }

    QString saveName = fi.fileName();
    QMessageBox::StandardButton reply = QMessageBox::question(this, tr("删除存档"),
        tr("确定要删除存档 '%1' 吗？\n该存档将被移动到回收站。").arg(saveName),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) {
        return;
    }

    if (InstanceManager::instance().moveSaveToTrash(saveFolderPath)) {
        // 静默刷新列表
        loadSaves();
    } else {
        QMessageBox::warning(this, tr("删除失败"), tr("无法删除存档，请检查文件是否被占用。"));
    }
}

void SavesTabPage::refreshIcons(const QString &color)
{
    Q_UNUSED(color);
    // 列表项仅含文字与箭头，无图标需要刷新
}
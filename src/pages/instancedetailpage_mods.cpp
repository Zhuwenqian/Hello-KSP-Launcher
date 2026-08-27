// 实例管理详情页 - 模组管理功能实现
#include "instancedetailpage.h"
#include "../ckanmanager.h"
#include "ckan/version.h"
#include "../iconutils.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QComboBox>
#include <QLineEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QScrollArea>
#include <QSet>
#include <QItemSelectionModel>
#include <QTextEdit>

namespace {
// 将关系列表转为模块名列表（用于依赖/冲突展示）
QStringList relNames(const QVector<ckan::Relationship> &rels)
{
    QStringList out;
    for (const ckan::Relationship &r : rels)
        out << r.name;
    return out;
}

QString formatBytes(qint64 bytes)
{
    if (bytes < 0) return QStringLiteral("?");
    if (bytes < 1024) return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QStringLiteral("%1 KB").arg(QString::number(bytes / 1024.0, 'f', 1));
    return QStringLiteral("%1 MB").arg(QString::number(bytes / 1024.0 / 1024.0, 'f', 1));
}
} // namespace

void InstanceDetailPage::setupModsTab()
{
    QWidget* tab = new QWidget(m_contentStack);
    tab->setObjectName("modsContentWidget");
    QVBoxLayout* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(15, 10, 15, 15);
    layout->setSpacing(8);

    // 顶栏：搜索 + 筛选 + 刷新仓库
    QWidget* topBar = new QWidget(tab);
    QHBoxLayout* topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(8);

    m_modSearchEdit = new QLineEdit(topBar);
    m_modSearchEdit->setPlaceholderText(tr("搜索模组名称或标识符..."));
    m_modSearchEdit->setClearButtonEnabled(true);
    m_modSearchEdit->setMinimumHeight(34);
    connect(m_modSearchEdit, &QLineEdit::textChanged, this, &InstanceDetailPage::onModSearchChanged);

    m_modFilterCombo = new QComboBox(topBar);
    m_modFilterCombo->addItem(tr("全部"), -1);
    m_modFilterCombo->addItem(tr("已安装"), static_cast<int>(ModsTableModel::Installed));
    m_modFilterCombo->addItem(tr("可升级"), static_cast<int>(ModsTableModel::Upgradable));
    m_modFilterCombo->addItem(tr("未安装"), static_cast<int>(ModsTableModel::NotInstalled));
    m_modFilterCombo->setMinimumHeight(34);
    connect(m_modFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &InstanceDetailPage::onModFilterChanged);

    m_refreshModsBtn = new QPushButton(IconUtils::tintedIcon(":/icons/refresh.svg", "#ffffff"),
                                       tr(" 刷新仓库"), topBar);
    m_refreshModsBtn->setObjectName("primaryButton");
    m_refreshModsBtn->setMinimumHeight(34);
    connect(m_refreshModsBtn, &QPushButton::clicked, this, &InstanceDetailPage::onRefreshModsClicked);

    m_selectAllBtn = new QPushButton(IconUtils::tintedIcon(":/icons/check.svg", "#ffffff"),
                                     tr(" 全选"), topBar);
    m_selectAllBtn->setMinimumHeight(34);
    connect(m_selectAllBtn, &QPushButton::clicked, this, &InstanceDetailPage::onSelectAllClicked);

    m_showIncompatCheck = new QCheckBox(tr("显示不兼容"), topBar);
    m_showIncompatCheck->setMinimumHeight(34);
    m_showIncompatCheck->setChecked(ConfigManager::instance().showIncompatibleMods());
    connect(m_showIncompatCheck, &QCheckBox::toggled,
            this, &InstanceDetailPage::onShowIncompatibleToggled);

    m_compatBtn = new QPushButton(IconUtils::tintedIcon(":/icons/rocket.svg", "#ffffff"),
                                  tr(" 兼容版本"), topBar);
    m_compatBtn->setMinimumHeight(34);
    connect(m_compatBtn, &QPushButton::clicked,
            this, &InstanceDetailPage::onCompatVersionsClicked);

    topLayout->addWidget(m_modSearchEdit, 1);
    topLayout->addWidget(m_modFilterCombo);
    topLayout->addWidget(m_showIncompatCheck);
    topLayout->addWidget(m_compatBtn);
    topLayout->addWidget(m_selectAllBtn);
    topLayout->addWidget(m_refreshModsBtn);
    layout->addWidget(topBar);

    // 表格
    m_modsModel = new ModsTableModel(this);
    m_modsProxy = new ModsFilterProxyModel(this);
    m_modsProxy->setSourceModel(m_modsModel);

    m_modTable = new QTableView(tab);
    m_modTable->setModel(m_modsProxy);
    m_modTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_modTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_modTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_modTable->verticalHeader()->setVisible(false);
    m_modTable->horizontalHeader()->setStretchLastSection(true);
    m_modTable->horizontalHeader()->setSectionResizeMode(ModsTableModel::ColCheck, QHeaderView::Fixed);
    m_modTable->setColumnWidth(ModsTableModel::ColCheck, 40);
    m_modTable->horizontalHeader()->setSectionResizeMode(ModsTableModel::ColName, QHeaderView::Stretch);
    m_modTable->horizontalHeader()->setSectionResizeMode(ModsTableModel::ColIdentifier, QHeaderView::ResizeToContents);
    m_modTable->horizontalHeader()->setSectionResizeMode(ModsTableModel::ColStatus, QHeaderView::ResizeToContents);
    m_modTable->horizontalHeader()->setSectionResizeMode(ModsTableModel::ColDownloads, QHeaderView::ResizeToContents);
    m_modTable->setSortingEnabled(true);
    m_modTable->sortByColumn(ModsTableModel::ColName, Qt::AscendingOrder);
    connect(m_modTable->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &InstanceDetailPage::onModSelectionChanged);
    connect(m_modTable, &QTableView::doubleClicked, this, &InstanceDetailPage::onModDoubleClicked);
    connect(m_modsModel, &ModsTableModel::dataChanged, this, [this]() {
        updateSelectAllButtonText();
        updateModActionButtons();
    });
    layout->addWidget(m_modTable, 1);

    // 下载进度条（任务进行中显示）
    m_modProgressWidget = new QWidget(tab);
    QHBoxLayout* progLayout = new QHBoxLayout(m_modProgressWidget);
    progLayout->setContentsMargins(0, 0, 0, 0);
    progLayout->setSpacing(8);
    m_modProgressBar = new QProgressBar(m_modProgressWidget);
    m_modProgressBar->setRange(0, 1000);
    m_modProgressBar->setValue(0);
    m_modProgressBar->setTextVisible(false);
    m_modProgressLabel = new QLabel(tr("就绪"), m_modProgressWidget);
    m_modProgressLabel->setMinimumWidth(220);
    m_cancelDownloadBtn = new QPushButton(IconUtils::tintedIcon(":/icons/x.svg", "#ffffff"),
                                          tr(" 取消"), m_modProgressWidget);
    m_cancelDownloadBtn->setObjectName("dangerButton");
    connect(m_cancelDownloadBtn, &QPushButton::clicked,
            this, &InstanceDetailPage::onCancelDownloadClicked);
    progLayout->addWidget(m_modProgressBar, 1);
    progLayout->addWidget(m_modProgressLabel);
    progLayout->addWidget(m_cancelDownloadBtn);
    m_modProgressWidget->setVisible(false);
    layout->addWidget(m_modProgressWidget);

    // 底部：详情 + 操作按钮
    m_modDetailText = new QTextEdit(tab);
    m_modDetailText->setReadOnly(true);
    m_modDetailText->setMaximumHeight(150);
    m_modDetailText->setPlaceholderText(tr("选中一个模组查看详情，双击查看依赖信息"));
    layout->addWidget(m_modDetailText);

    QWidget* btnBar = new QWidget(tab);
    QHBoxLayout* btnLayout = new QHBoxLayout(btnBar);
    btnLayout->setContentsMargins(0, 0, 0, 0);
    btnLayout->setSpacing(8);

    m_installModBtn = new QPushButton(IconUtils::tintedIcon(":/icons/add.svg", "#ffffff"),
                                      tr(" 安装"), btnBar);
    m_installModBtn->setObjectName("primaryButton");
    m_installModBtn->setMinimumHeight(36);
    connect(m_installModBtn, &QPushButton::clicked, this, &InstanceDetailPage::onInstallModClicked);

    m_upgradeModBtn = new QPushButton(IconUtils::tintedIcon(":/icons/check.svg", "#ffffff"),
                                      tr(" 升级"), btnBar);
    m_upgradeModBtn->setObjectName("primaryButton");
    m_upgradeModBtn->setMinimumHeight(36);
    connect(m_upgradeModBtn, &QPushButton::clicked, this, &InstanceDetailPage::onUpgradeModClicked);

    m_uninstallModBtn = new QPushButton(IconUtils::tintedIcon(":/icons/trash-2.svg", "#ffffff"),
                                        tr(" 卸载"), btnBar);
    m_uninstallModBtn->setObjectName("dangerButton");
    m_uninstallModBtn->setMinimumHeight(36);
    connect(m_uninstallModBtn, &QPushButton::clicked, this, &InstanceDetailPage::onUninstallModClicked);

    btnLayout->addStretch();
    btnLayout->addWidget(m_installModBtn);
    btnLayout->addWidget(m_upgradeModBtn);
    btnLayout->addWidget(m_uninstallModBtn);
    layout->addWidget(btnBar);

    // 适配层信号
    connect(&CKanManager::instance(), &CKanManager::indexRefreshed,
            this, &InstanceDetailPage::onIndexRefreshed);
    connect(&CKanManager::instance(), &CKanManager::unmanagedScanFinished,
            this, &InstanceDetailPage::onUnmanagedScanFinished);
    connect(&CKanManager::instance(), &CKanManager::operationFinished,
            this, &InstanceDetailPage::onModOperationFinished);
    connect(&CKanManager::instance(), &CKanManager::installProgress,
            this, [this](const QString &id, int percent) {
        if (id == m_currentModIdentifier)
            m_modDetailText->setPlainText(tr("正在处理 %1 ... %2%").arg(id).arg(percent));
        // 进入安装阶段：进度条切为不确定模式，文案改为“正在安装”
        if (!m_modProgressWidget->isVisible())
            m_modProgressWidget->setVisible(true);
        m_modProgressBar->setRange(0, 0);
        m_modProgressLabel->setText(tr("正在安装：%1").arg(id));
        m_cancelDownloadBtn->setEnabled(true);
    });
    connect(&CKanManager::instance(), &CKanManager::downloadProgress,
            this, &InstanceDetailPage::onDownloadProgress);

    updateModActionButtons();
    m_contentStack->addWidget(tab);
}

void InstanceDetailPage::prepareMods()
{
    if (m_instance.path.isEmpty()) return;

    CKanManager &mgr = CKanManager::instance();
    // 绑定实例（同一实例时内部直接返回，不会重复构造/扫描）
    mgr.openInstance(m_instance.path, m_instance.name);

    if (m_modsProxy) {
        m_modsProxy->setShowIncompatible(ConfigManager::instance().showIncompatibleMods());
        // 传入实际检测到的 KSP 版本，按真实游戏版本过滤不兼容模组
        m_modsProxy->setGameVersion(mgr.detectedVersion());
    }
    // 应用用户勾选的兼容版本区间（过滤代理 + 安装/依赖解析共用）
    applyCompatRange();

    // 手动安装模组（AD）DLL 扫描：后台线程执行，结果缓存，避免阻塞 UI 与重复全盘扫描
    if (!mgr.unmanagedScanDone()) {
        m_modsModel->clear();
        mgr.scanUnmanagedDllsAsync();
    }

    if (!mgr.indexReady()) {
        // 首次进入：自动加载仓库索引（优先使用本地缓存）
        m_modsModel->clear();
        m_modDetailText->setPlainText(tr("正在加载 CKAN 仓库索引，请稍候..."));
        showDownloadProgress();
        mgr.refreshIndexAsync();
    }

    maybePopulateMods();
}

// 索引与 DLL 扫描均就绪时填充模组模型；否则保持清空并显示"加载中"提示。
void InstanceDetailPage::maybePopulateMods()
{
    CKanManager &mgr = CKanManager::instance();
    m_modsReady = mgr.indexReady() && mgr.unmanagedScanDone();
    if (m_modsReady) {
        m_modsModel->setModules(mgr.search(QString()));
        updateModActionButtons();
        return;
    }
    // 数据尚未就绪：若用户已切到模组页，给出明确的"加载中"提示
    m_modsModel->clear();
    if (m_modsTabActive) {
        if (!mgr.indexReady())
            m_modDetailText->setPlainText(tr("正在加载 CKAN 仓库索引，请稍候..."));
        else
            m_modDetailText->setPlainText(tr("正在扫描已安装的 DLL，请稍候..."));
    }
}

void InstanceDetailPage::onUnmanagedScanFinished()
{
    // 后台 DLL 扫描完成：若索引也已就绪则填充模型（含此前切到模组页的场景）
    maybePopulateMods();
    updateModActionButtons();
}

void InstanceDetailPage::onModSearchChanged(const QString &text)
{
    if (m_modsProxy) m_modsProxy->setSearchText(text);
}

void InstanceDetailPage::onModFilterChanged(int index)
{
    if (!m_modsProxy || !m_modFilterCombo) return;
    m_modsProxy->setStatusFilter(m_modFilterCombo->itemData(index).toInt());
}

void InstanceDetailPage::onShowIncompatibleToggled(bool checked)
{
    ConfigManager::instance().setShowIncompatibleMods(checked);
    if (m_modsProxy) m_modsProxy->setShowIncompatible(checked);
}

// 将当前实例勾选的兼容版本区间应用到过滤代理与 CKanManager（供安装/依赖解析使用）
void InstanceDetailPage::applyCompatRange()
{
    if (m_instanceId.isEmpty()) return;
    // 未配置时按检测到的游戏版本动态推导默认勾选（1.9~1.12 区间内回退到 1.9）
    const ckan::GameVersion detected = CKanManager::instance().detectedVersion();
    const QStringList lines = ConfigManager::instance().compatibleVersions(m_instanceId, detected);
    const ckan::GameVersionRange range = ckan::versionLinesToRange(lines);
    if (m_modsProxy)
        m_modsProxy->setCompatRange(range);
    CKanManager::instance().setCompatRange(range);
    if (m_compatBtn) {
        // 按钮文案显示当前已勾选的版本数，空勾选则仅按实例实际版本判断
        if (lines.isEmpty()) {
            m_compatBtn->setText(tr(" 兼容版本"));
            m_compatBtn->setToolTip(tr("仅按当前实例实际版本判断兼容性"));
        } else {
            m_compatBtn->setText(tr(" 兼容版本(%1)").arg(lines.size()));
            m_compatBtn->setToolTip(tr("已勾选：%1").arg(lines.join(QStringLiteral("、"))));
        }
    }
}

// 兼容版本设置弹窗：勾选 1.0~1.12 全部版本线（未配置时按游戏版本动态推导默认，
// 1.9~1.12 区间内回退到 1.9，低于 1.9 仅勾选自身版本线），
// 勾选后 KSP 版本落在所选连续区间内的模组均视为兼容；全部取消则仅按实例实际版本判断。
void InstanceDetailPage::onCompatVersionsClicked()
{
    if (m_instanceId.isEmpty()) return;

    QDialog dlg(this);
    dlg.setWindowTitle(tr("兼容版本设置"));
    dlg.setMinimumWidth(360);
    QVBoxLayout *lay = new QVBoxLayout(&dlg);

    QLabel *info = new QLabel(tr("勾选需要兼容的 KSP 版本（1.0 ~ 1.12）。\n"
                                 "勾选后，KSP 版本落在所选区间内的模组均视为兼容。\n"
                                 "全部取消勾选则仅按当前实例实际版本判断。"), &dlg);
    info->setWordWrap(true);
    lay->addWidget(info);

    // 当前实例已保存的勾选（未配置时按检测到的游戏版本动态推导默认勾选）
    const QStringList saved = ConfigManager::instance().compatibleVersions(
        m_instanceId, CKanManager::instance().detectedVersion());
    const QSet<QString> savedSet(saved.begin(), saved.end());

    QScrollArea *scroll = new QScrollArea(&dlg);
    scroll->setWidgetResizable(true);
    QWidget *host = new QWidget(scroll);
    QVBoxLayout *hostLay = new QVBoxLayout(host);
    QVector<QCheckBox*> boxes;
    // 全部版本：1.0 ~ 1.12
    for (int minor = 0; minor <= 12; ++minor) {
        const QString line = QStringLiteral("1.%1").arg(minor);
        QCheckBox *cb = new QCheckBox(tr("KSP %1").arg(line), host);
        cb->setChecked(savedSet.contains(line));
        boxes.append(cb);
        hostLay->addWidget(cb);
    }
    hostLay->addStretch();
    scroll->setWidget(host);
    lay->addWidget(scroll, 1);

    QDialogButtonBox *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    btnBox->button(QDialogButtonBox::Ok)->setText(tr("确定"));
    btnBox->button(QDialogButtonBox::Cancel)->setText(tr("取消"));
    connect(btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(btnBox);

    if (dlg.exec() != QDialog::Accepted) return;

    QStringList selected;
    for (int minor = 0; minor <= 12; ++minor)
        if (boxes.at(minor)->isChecked())
            selected << QStringLiteral("1.%1").arg(minor);

    ConfigManager::instance().setCompatibleVersions(m_instanceId, selected);
    applyCompatRange();
}

void InstanceDetailPage::onRefreshModsClicked()
{
    if (m_instance.path.isEmpty()) return;
    CKanManager::instance().openInstance(m_instance.path, m_instance.name);
    m_modDetailText->setPlainText(tr("正在刷新仓库索引..."));
    showDownloadProgress();
    CKanManager::instance().refreshIndexAsync(true); // 手动刷新：强制重新下载
}

void InstanceDetailPage::onIndexRefreshed(bool ok, const QString &error)
{
    hideDownloadProgress();
    if (error == QStringLiteral("已取消")) {
        m_modDetailText->setPlainText(tr("已取消仓库索引加载。"));
        return;
    }
    if (!ok) {
        m_modsModel->clear();
        m_modDetailText->setPlainText(tr("仓库索引刷新失败：%1").arg(error));
        QMessageBox::warning(this, tr("刷新失败"), tr("无法获取仓库索引：\n%1").arg(error));
        return;
    }
    // 索引已就绪：若 DLL 扫描也完成则填充模型并显示数量；否则保留"加载中"提示，
    // 待后台扫描完成（unmanagedScanFinished）后再填充。
    maybePopulateMods();
    if (m_modsReady) {
        const int n = m_modsModel->rowCount();
        m_modDetailText->setPlainText(tr("仓库索引已就绪，共 %1 个模组。").arg(n));
    }
    updateModActionButtons();
    // 索引就绪后，若存在 .ckan 导入待装清单，自动开始批量安装
    if (!m_pendingCkanIdentifiers.isEmpty()) {
        const QStringList pending = m_pendingCkanIdentifiers;
        m_pendingCkanIdentifiers.clear();
        CKanManager::instance().installBatchAsync(pending);
    }
    // 部分仓库获取失败：提示用户（避免“静默缺失”）；页面不可见（如在设置页）时不弹窗
    if (!error.isEmpty() && isVisible())
        QMessageBox::warning(this, tr("仓库刷新"),
                             tr("部分仓库获取失败，已用其他仓库/旧缓存：\n%1").arg(error));
}

void InstanceDetailPage::onModSelectionChanged()
{
    if (!m_modsProxy || !m_modTable) return;
    const QModelIndex idx = m_modTable->currentIndex();
    if (!idx.isValid()) {
        m_currentModIdentifier.clear();
        m_modDetailText->clear();
        updateModActionButtons();
        return;
    }
    const QModelIndex src = m_modsProxy->mapToSource(idx);
    const ckan::CkanModule mod = m_modsModel->moduleAt(src.row());
    m_currentModIdentifier = mod.identifier;
    showModDetails(mod);
    updateModActionButtons();
}

void InstanceDetailPage::onModDoubleClicked(const QModelIndex &index)
{
    if (!m_modsProxy || !index.isValid()) return;
    const QModelIndex src = m_modsProxy->mapToSource(index);
    const ckan::CkanModule mod = m_modsModel->moduleAt(src.row());
    if (!mod.isValid()) return;
    // 双击：显示依赖与冲突详情
    QString text = mod.name + "  " + mod.version + "\n\n";
    text += tr("依赖：") + (mod.depends.isEmpty() ? tr("（无）")
           : relNames(mod.depends).join(QStringLiteral(", "))) + "\n";
    text += tr("推荐：") + (mod.recommends.isEmpty() ? tr("（无）")
           : relNames(mod.recommends).join(QStringLiteral(", "))) + "\n";
    text += tr("冲突：") + (mod.conflicts.isEmpty() ? tr("（无）")
           : relNames(mod.conflicts).join(QStringLiteral(", ")));
    m_modDetailText->setPlainText(text);
}

void InstanceDetailPage::updateModActionButtons()
{
    if (!m_installModBtn || !m_uninstallModBtn || !m_upgradeModBtn) return;
    CKanManager &mgr = CKanManager::instance();
    const QStringList checked = m_modsModel->checkedIdentifiers();
    const bool batch = checked.size() >= 2;

    // 按钮文案：批量时显示数量
    m_installModBtn->setText(batch ? tr(" 安装 (%1)").arg(checked.size()) : tr(" 安装"));
    m_upgradeModBtn->setText(batch ? tr(" 升级 (%1)").arg(checked.size()) : tr(" 升级"));
    m_uninstallModBtn->setText(batch ? tr(" 卸载 (%1)").arg(checked.size()) : tr(" 卸载"));

    m_installModBtn->setEnabled(false);
    m_upgradeModBtn->setEnabled(false);
    m_uninstallModBtn->setEnabled(false);

    if (batch) {
        bool anyInstall = false, anyUpgrade = false, anyUninstall = false;
        for (const QString &id : checked) {
            if (mgr.isAutoDetected(id)) continue;
            const bool installed = mgr.isInstalled(id);
            const bool upgradable = mgr.isUpgradable(id);
            if (!installed || upgradable) anyInstall = true;
            if (upgradable) anyUpgrade = true;
            if (installed) anyUninstall = true;
        }
        m_installModBtn->setEnabled(anyInstall);
        m_upgradeModBtn->setEnabled(anyUpgrade);
        m_uninstallModBtn->setEnabled(anyUninstall);
        return;
    }

    // 单个：勾选 1 个时用勾选的模组，否则用当前选中行
    const QString target = checked.size() == 1 ? checked.first() : m_currentModIdentifier;
    if (target.isEmpty()) return;
    if (mgr.isAutoDetected(target)) return;
    const bool installed = mgr.isInstalled(target);
    const bool upgradable = mgr.isUpgradable(target);
    m_installModBtn->setEnabled(!installed || upgradable);
    m_uninstallModBtn->setEnabled(installed && !mgr.indexReady() ? true : installed);
    m_upgradeModBtn->setEnabled(upgradable);
}

void InstanceDetailPage::showModDetails(const ckan::CkanModule &mod)
{
    if (!mod.isValid()) return;
    QString text;
    text += "<b>" + mod.name.toHtmlEscaped() + "  " + mod.version.toHtmlEscaped() + "</b>\n";
    if (!mod.abstract.isEmpty())
        text += tr("描述：%1\n").arg(mod.abstract.toHtmlEscaped());
    if (!mod.author.isEmpty()) text += tr("作者：%1\n").arg(mod.author.join(QStringLiteral(", ")));
    if (!mod.license.isEmpty()) text += tr("许可：%1\n").arg(mod.license.join(QStringLiteral(", ")));
    if (!mod.kspVersion.isEmpty()) text += tr("KSP 版本：%1\n").arg(mod.kspVersion);
    if (mod.downloadSize > 0)
        text += tr("下载大小：%1 MB\n").arg(QString::number(mod.downloadSize / 1024.0 / 1024.0, 'f', 1));
    if (!mod.depends.isEmpty()) text += tr("依赖：%1\n").arg(relNames(mod.depends).join(QStringLiteral(", ")));
    if (!mod.conflicts.isEmpty()) text += tr("冲突：%1").arg(relNames(mod.conflicts).join(QStringLiteral(", ")));
    // QTextEdit 的 HTML 会把裸换行符折叠为空格，需换成 <br/> 才能真正换行
    text.replace(QStringLiteral("\n"), QStringLiteral("<br/>"));
    m_modDetailText->setHtml(text);
}

void InstanceDetailPage::onInstallModClicked()
{
    const QStringList ids = m_modsModel->checkedIdentifiers();
    if (ids.size() >= 2) {
        setModButtonsEnabled(false);
        showDownloadProgress();
        CKanManager::instance().installBatchAsync(ids);
        return;
    }
    const QString target = ids.size() == 1 ? ids.first() : m_currentModIdentifier;
    if (target.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请先选择或勾选一个模组。"));
        return;
    }
    m_refreshModsBtn->setEnabled(false);
    m_installModBtn->setEnabled(false);
    m_uninstallModBtn->setEnabled(false);
    m_upgradeModBtn->setEnabled(false);
    showDownloadProgress();
    CKanManager::instance().installAsync(target, true);
}

void InstanceDetailPage::onUninstallModClicked()
{
    const QStringList ids = m_modsModel->checkedIdentifiers();
    if (ids.size() >= 2) {
        if (QMessageBox::question(this, tr("确认批量卸载"),
                tr("确定要卸载已勾选的 %1 个模组吗？").arg(ids.size()),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
            return;
        }
        setModButtonsEnabled(false);
        CKanManager::instance().uninstallBatchAsync(ids);
        return;
    }
    const QString target = ids.size() == 1 ? ids.first() : m_currentModIdentifier;
    if (target.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请先选择或勾选一个模组。"));
        return;
    }
    QString name = target;
    for (int r = 0; r < m_modsProxy->rowCount(); ++r) {
        const QModelIndex src = m_modsProxy->mapToSource(m_modsProxy->index(r, ModsTableModel::ColCheck));
        const ckan::CkanModule mod = m_modsModel->moduleAt(src.row());
        if (mod.isValid() && mod.identifier == target) { name = mod.name; break; }
    }
    if (QMessageBox::question(this, tr("确认卸载"),
            tr("确定要卸载模组 %1 吗？").arg(name.isEmpty() ? target : name),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
        return;
    }
    m_refreshModsBtn->setEnabled(false);
    m_installModBtn->setEnabled(false);
    m_uninstallModBtn->setEnabled(false);
    m_upgradeModBtn->setEnabled(false);
    CKanManager::instance().uninstallAsync(target);
}

void InstanceDetailPage::onUpgradeModClicked()
{
    const QStringList ids = m_modsModel->checkedIdentifiers();
    if (ids.size() >= 2) {
        setModButtonsEnabled(false);
        showDownloadProgress();
        CKanManager::instance().upgradeBatchAsync(ids);
        return;
    }
    const QString target = ids.size() == 1 ? ids.first() : m_currentModIdentifier;
    if (target.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请先选择或勾选一个模组。"));
        return;
    }
    setModButtonsEnabled(false);
    showDownloadProgress();
    CKanManager::instance().upgradeAsync(target);
}

void InstanceDetailPage::setModButtonsEnabled(bool enabled)
{
    m_refreshModsBtn->setEnabled(enabled);
    m_installModBtn->setEnabled(enabled);
    m_uninstallModBtn->setEnabled(enabled);
    m_upgradeModBtn->setEnabled(enabled);
    if (!enabled) return;
    updateModActionButtons();
    updateSelectAllButtonText();
}

void InstanceDetailPage::updateSelectAllButtonText()
{
    if (!m_selectAllBtn) return;
    bool any = false, all = true;
    for (int r = 0; r < m_modsProxy->rowCount(); ++r) {
        const QModelIndex src = m_modsProxy->mapToSource(m_modsProxy->index(r, ModsTableModel::ColCheck));
        const ckan::CkanModule m = m_modsModel->moduleAt(src.row());
        if (!m.isValid()) continue;
        any = true;
        if (!m_modsModel->isChecked(m.identifier)) { all = false; break; }
    }
    m_selectAllBtn->setText(any && all ? tr(" 清空") : tr(" 全选"));
}

void InstanceDetailPage::onSelectAllClicked()
{
    QVector<ckan::CkanModule> visible;
    for (int r = 0; r < m_modsProxy->rowCount(); ++r) {
        const QModelIndex src = m_modsProxy->mapToSource(m_modsProxy->index(r, ModsTableModel::ColCheck));
        const ckan::CkanModule m = m_modsModel->moduleAt(src.row());
        if (m.isValid()) visible.append(m);
    }
    if (visible.isEmpty()) return;
    bool allChecked = true;
    for (const ckan::CkanModule &m : visible)
        if (!m_modsModel->isChecked(m.identifier)) { allChecked = false; break; }
    m_modsModel->setAllChecked(visible, !allChecked);
    updateSelectAllButtonText();
    updateModActionButtons();
}

void InstanceDetailPage::onModOperationFinished(bool ok, const QString &message)
{
    setModButtonsEnabled(true);
    hideDownloadProgress();
    if (ok) {
        m_modsModel->refreshStatus();
        m_modsModel->clearAllChecks();
        updateSelectAllButtonText();
        updateModActionButtons();
        // 重新显示当前选中模组详情，覆盖安装期间残留的“正在处理 ... %”进度文案
        onModSelectionChanged();
        QMessageBox::information(this, tr("完成"), message);
    } else {
        updateModActionButtons();
        QMessageBox::warning(this, tr("操作失败"), message);
    }
}

void InstanceDetailPage::showDownloadProgress()
{
    m_modProgressBar->setRange(0, 1000);
    m_modProgressBar->setValue(0);
    m_modProgressLabel->setText(tr("准备下载..."));
    m_cancelDownloadBtn->setEnabled(true);
    m_modProgressWidget->setVisible(true);
}

void InstanceDetailPage::hideDownloadProgress()
{
    m_modProgressWidget->setVisible(false);
}

void InstanceDetailPage::onDownloadProgress(const QString &identifier, qint64 doneBytes,
                                            qint64 totalBytes, qint64 speedBps)
{
    if (!m_modProgressWidget->isVisible())
        m_modProgressWidget->setVisible(true);
    if (totalBytes > 0) {
        // 已知总量：按字节计算百分比
        if (m_modProgressBar->maximum() == 0)
            m_modProgressBar->setRange(0, 1000);
        const int v = static_cast<int>(doneBytes * 1000 / totalBytes);
        m_modProgressBar->setValue(qBound(0, v, 1000));
    } else if (m_modProgressBar->maximum() != 0) {
        // 总量未知（如索引 tar.gz 分块下载）：不确定进度条
        m_modProgressBar->setRange(0, 0);
    }
    QString text = tr("正在下载：%1  %2 / %3")
        .arg(identifier, formatBytes(doneBytes), formatBytes(totalBytes));
    if (speedBps > 0)
        text += QStringLiteral("  (%1/s)").arg(formatBytes(speedBps));
    m_modProgressLabel->setText(text);
}

void InstanceDetailPage::onCancelDownloadClicked()
{
    CKanManager::instance().cancelCurrentOperation();
    m_modProgressLabel->setText(tr("正在取消..."));
    m_cancelDownloadBtn->setEnabled(false);
}

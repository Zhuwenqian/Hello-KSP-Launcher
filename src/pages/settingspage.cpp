#include "settingspage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QFormLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QIcon>
#include <QPixmap>
#include <QDir>
#include <QFile>
#include <QMenu>
#include <QDialog>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QSet>
#include "../backgroundmanager.h"
#include "../ckanmanager.h"

SettingsPage::SettingsPage(QWidget *parent)
    : QWidget(parent),
      m_backgroundPathLabel(nullptr),
      m_backgroundPreviewLabel(nullptr),
      m_chooseBackgroundBtn(nullptr),
      m_resetBackgroundBtn(nullptr),
      m_indexIntervalCombo(nullptr),
      m_indexSourceCombo(nullptr),
      m_moduleSourceCombo(nullptr),
      m_concurrencyCombo(nullptr),
      m_cacheDirLabel(nullptr),
      m_installSuggestsToggle(nullptr)
{
    setupUI();
    loadSettings();
    refreshBackgroundPreview();
    connect(&ConfigManager::instance(), &ConfigManager::configChanged, this, &SettingsPage::loadSettings);
    connect(&ConfigManager::instance(), &ConfigManager::configChanged, this, &SettingsPage::refreshBackgroundPreview);
    connect(&BackgroundManager::instance(), &BackgroundManager::backgroundChanged,
            this, &SettingsPage::refreshBackgroundPreview);
}

void SettingsPage::setupUI()
{
    // 外层滚动区域,避免默认窗口下内容超出被裁剪
    QVBoxLayout* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    rootLayout->addWidget(m_scrollArea);

    QWidget* container = new QWidget(m_scrollArea);
    container->setObjectName("settingsContent");
    m_scrollArea->setWidget(container);

    QVBoxLayout* mainLayout = new QVBoxLayout(container);
    mainLayout->setContentsMargins(20, 10, 20, 20);
    mainLayout->setSpacing(15);

    QLabel* titleLabel = new QLabel(tr("启动器设置"), this);
    titleLabel->setObjectName("pageTitle");
    mainLayout->addWidget(titleLabel);

    // ---- 通用设置 ----
    QGroupBox* generalGroup = new QGroupBox(tr("通用设置"), this);
    QFormLayout* generalLayout = new QFormLayout(generalGroup);
    generalLayout->setContentsMargins(20, 20, 20, 20);
    generalLayout->setSpacing(15);

    m_languageCombo = new QComboBox(generalGroup);
    m_languageCombo->addItem(QIcon(":/icons/globe.svg"), tr("简体中文"), "zh_CN");
    m_languageCombo->addItem(QIcon(":/icons/globe.svg"), tr("English"), "en_US");
    connect(m_languageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsPage::onLanguageChanged);

    QLabel* langLabel = new QLabel(tr("启动器语言："), generalGroup);
    langLabel->setObjectName("settingLabel");
    generalLayout->addRow(langLabel, m_languageCombo);

    m_behaviorCombo = new QComboBox(generalGroup);
    m_behaviorCombo->addItem(tr("保持窗口打开"), static_cast<int>(ConfigManager::KeepOpen));
    m_behaviorCombo->addItem(tr("最小化到任务栏"), static_cast<int>(ConfigManager::Minimize));
    m_behaviorCombo->addItem(tr("自动关闭启动器"), static_cast<int>(ConfigManager::Close));
    connect(m_behaviorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsPage::onLaunchBehaviorChanged);

    QLabel* behaviorLabel = new QLabel(tr("游戏启动后行为："), generalGroup);
    behaviorLabel->setObjectName("settingLabel");
    generalLayout->addRow(behaviorLabel, m_behaviorCombo);

    m_themeCombo = new QComboBox(generalGroup);
    m_themeCombo->addItem(QIcon(":/icons/moon.svg"), tr("深色主题"), "dark");
    m_themeCombo->addItem(QIcon(":/icons/sun.svg"), tr("浅色主题"), "light");
    connect(m_themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsPage::onThemeChanged);

    QLabel* themeLabel = new QLabel(tr("主题："), generalGroup);
    themeLabel->setObjectName("settingLabel");
    generalLayout->addRow(themeLabel, m_themeCombo);

    mainLayout->addWidget(generalGroup);

    // ---- 背景图片设置 ----
    QGroupBox* bgGroup = new QGroupBox(tr("背景图片"), this);
    QVBoxLayout* bgLayout = new QVBoxLayout(bgGroup);
    bgLayout->setContentsMargins(20, 20, 20, 20);
    bgLayout->setSpacing(12);

    QLabel* bgHint = new QLabel(tr("选择一张图片作为启动器背景。支持 PNG / JPG / JPEG。"), bgGroup);
    bgHint->setObjectName("settingHint");
    bgHint->setWordWrap(true);
    bgLayout->addWidget(bgHint);

    // 预览
    m_backgroundPreviewLabel = new QLabel(bgGroup);
    m_backgroundPreviewLabel->setObjectName("backgroundPreview");
    m_backgroundPreviewLabel->setFixedHeight(140);
    m_backgroundPreviewLabel->setAlignment(Qt::AlignCenter);
    m_backgroundPreviewLabel->setText(tr("（无预览）"));
    bgLayout->addWidget(m_backgroundPreviewLabel);

    // 当前路径
    QHBoxLayout* pathRow = new QHBoxLayout();
    QLabel* pathLabel = new QLabel(tr("当前背景："), bgGroup);
    pathLabel->setObjectName("settingLabel");
    m_backgroundPathLabel = new QLabel(bgGroup);
    m_backgroundPathLabel->setObjectName("settingLabel");
    m_backgroundPathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_backgroundPathLabel->setWordWrap(true);
    pathRow->addWidget(pathLabel, 0);
    pathRow->addWidget(m_backgroundPathLabel, 1);
    bgLayout->addLayout(pathRow);

    // 按钮
    QHBoxLayout* btnRow = new QHBoxLayout();
    m_chooseBackgroundBtn = new QPushButton(QIcon(":/icons/folder-open.svg"), tr(" 选择图片..."), bgGroup);
    m_chooseBackgroundBtn->setObjectName("primaryButton");
    m_chooseBackgroundBtn->setMinimumHeight(36);
    connect(m_chooseBackgroundBtn, &QPushButton::clicked,
            this, &SettingsPage::onChooseBackgroundClicked);

    m_resetBackgroundBtn = new QPushButton(tr("重置为默认"), bgGroup);
    m_resetBackgroundBtn->setMinimumHeight(36);
    connect(m_resetBackgroundBtn, &QPushButton::clicked,
            this, &SettingsPage::onResetBackgroundClicked);

    btnRow->addWidget(m_chooseBackgroundBtn);
    btnRow->addWidget(m_resetBackgroundBtn);
    btnRow->addStretch();
    bgLayout->addLayout(btnRow);

    mainLayout->addWidget(bgGroup);

    // ---- 模组管理设置 ----
    QGroupBox* modGroup = new QGroupBox(tr("模组管理"), this);
    QFormLayout* modLayout = new QFormLayout(modGroup);
    modLayout->setContentsMargins(20, 20, 20, 20);
    modLayout->setSpacing(15);

    // 下拉框宽度由 QFormLayout 自动拉伸填满可用空间

    // 索引刷新间隔
    m_indexIntervalCombo = new QComboBox(modGroup);
    m_indexIntervalCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_indexIntervalCombo->addItem(tr("6 小时"), 6 * 3600);
    m_indexIntervalCombo->addItem(tr("1 天"), 24 * 3600);
    m_indexIntervalCombo->addItem(tr("3 天"), 3 * 24 * 3600);
    m_indexIntervalCombo->addItem(tr("5 天"), 5 * 24 * 3600);
    connect(m_indexIntervalCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsPage::onIndexIntervalChanged);
    QLabel* intervalLabel = new QLabel(tr("索引刷新间隔："), modGroup);
    intervalLabel->setObjectName("settingLabel");
    modLayout->addRow(intervalLabel, m_indexIntervalCombo);

    // 索引下载源
    m_indexSourceCombo = new QComboBox(modGroup);
    m_indexSourceCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_indexSourceCombo->addItem(tr("尽量选择官方源"), static_cast<int>(ConfigManager::OfficialFirst));
    m_indexSourceCombo->addItem(tr("尽量选择镜像源"), static_cast<int>(ConfigManager::MirrorFirst));
    connect(m_indexSourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsPage::onIndexSourceChanged);
    QLabel* indexSrcLabel = new QLabel(tr("索引下载源："), modGroup);
    indexSrcLabel->setObjectName("settingLabel");
    modLayout->addRow(indexSrcLabel, m_indexSourceCombo);

    // 模组下载源
    m_moduleSourceCombo = new QComboBox(modGroup);
    m_moduleSourceCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_moduleSourceCombo->addItem(tr("尽量选择官方源"), static_cast<int>(ConfigManager::OfficialFirst));
    m_moduleSourceCombo->addItem(tr("尽量选择镜像源"), static_cast<int>(ConfigManager::MirrorFirst));
    connect(m_moduleSourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsPage::onModuleSourceChanged);
    QLabel* modSrcLabel = new QLabel(tr("模组下载源："), modGroup);
    modSrcLabel->setObjectName("settingLabel");
    modLayout->addRow(modSrcLabel, m_moduleSourceCombo);

    // 下载并发数
    m_concurrencyCombo = new QComboBox(modGroup);
    m_concurrencyCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    for (int i = 1; i <= 8; ++i)
        m_concurrencyCombo->addItem(tr("同时下载 %1 个").arg(i), i);
    connect(m_concurrencyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsPage::onConcurrencyChanged);
    QLabel* concLabel = new QLabel(tr("下载并发数："), modGroup);
    concLabel->setObjectName("settingLabel");
    modLayout->addRow(concLabel, m_concurrencyCombo);

    // 安装时显示建议模组
    m_installSuggestsToggle = new ToggleSwitch(modGroup);
    m_installSuggestsToggle->setToolTip(tr("安装模组时，如果它还有建议安装的可选模组，弹窗勾选"));
    connect(m_installSuggestsToggle, &ToggleSwitch::toggled, this,
            [](bool checked) { ConfigManager::instance().setInstallSuggests(checked); });
    QLabel* suggestLabel = new QLabel(tr("安装时显示建议模组："), modGroup);
    suggestLabel->setObjectName("settingLabel");
    modLayout->addRow(suggestLabel, m_installSuggestsToggle);

    // 下载缓存文件夹
    QHBoxLayout* cacheRow = new QHBoxLayout();
    m_cacheDirLabel = new QLabel(modGroup);
    m_cacheDirLabel->setObjectName("settingLabel");
    m_cacheDirLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_cacheDirLabel->setWordWrap(true);
    QPushButton* chooseCacheBtn = new QPushButton(tr("选择文件夹..."), modGroup);
    chooseCacheBtn->setMinimumHeight(32);
    connect(chooseCacheBtn, &QPushButton::clicked, this, &SettingsPage::onChooseCacheDirClicked);
    QPushButton* clearCacheBtn = new QPushButton(tr("清理缓存"), modGroup);
    clearCacheBtn->setMinimumHeight(32);
    connect(clearCacheBtn, &QPushButton::clicked, this, &SettingsPage::onClearCacheClicked);
    cacheRow->addWidget(m_cacheDirLabel, 1);
    cacheRow->addWidget(chooseCacheBtn);
    cacheRow->addWidget(clearCacheBtn);
    QLabel* cacheLabel = new QLabel(tr("下载缓存文件夹："), modGroup);
    cacheLabel->setObjectName("settingLabel");
    modLayout->addRow(cacheLabel, cacheRow);

    mainLayout->addWidget(modGroup);

    // ---- 仓库列表 ----
    QGroupBox* repoGroup = new QGroupBox(tr("仓库列表"), this);
    QVBoxLayout* repoLayout = new QVBoxLayout(repoGroup);
    repoLayout->setContentsMargins(20, 20, 20, 20);
    repoLayout->setSpacing(10);

    QLabel* repoHint = new QLabel(tr("多个仓库的模组会按优先级合并，排在上方的优先级更高（冲突时优先采用）。修改仓库后会自动刷新索引。"), repoGroup);
    repoHint->setObjectName("settingHint");
    repoHint->setWordWrap(true);
    repoLayout->addWidget(repoHint);

    m_repoList = new QListWidget(repoGroup);
    m_repoList->setMinimumHeight(120);
    m_repoList->setAlternatingRowColors(true);
    repoLayout->addWidget(m_repoList);

    QHBoxLayout* repoBtnRow = new QHBoxLayout();
    QPushButton* addPresetBtn = new QPushButton(tr("添加预设..."), repoGroup);
    addPresetBtn->setMinimumHeight(32);
    QMenu* presetMenu = new QMenu(addPresetBtn);
    QAction* actBackup = presetMenu->addAction(tr("KSP-CKAN 备用仓库"));
    QAction* actSol = presetMenu->addAction(tr("Sol 仓库"));
    QAction* actMJ2 = presetMenu->addAction(tr("MechJeb2-dev 仓库"));
    connect(actBackup, &QAction::triggered, this, [this]() {
        onAddPresetRepo(ckan::Repository::presetKspCkanBackup());
    });
    connect(actSol, &QAction::triggered, this, [this]() {
        onAddPresetRepo(ckan::Repository::presetSol());
    });
    connect(actMJ2, &QAction::triggered, this, [this]() {
        onAddPresetRepo(ckan::Repository::presetMechJeb2Dev());
    });
    addPresetBtn->setMenu(presetMenu);
    repoBtnRow->addWidget(addPresetBtn);

    QPushButton* addCustomBtn = new QPushButton(tr("自定义添加..."), repoGroup);
    addCustomBtn->setMinimumHeight(32);
    connect(addCustomBtn, &QPushButton::clicked, this, &SettingsPage::onAddCustomRepo);
    repoBtnRow->addWidget(addCustomBtn);

    QPushButton* removeBtn = new QPushButton(tr("删除"), repoGroup);
    removeBtn->setMinimumHeight(32);
    connect(removeBtn, &QPushButton::clicked, this, &SettingsPage::onRemoveRepo);
    repoBtnRow->addWidget(removeBtn);

    QPushButton* upBtn = new QPushButton(tr("上移"), repoGroup);
    upBtn->setMinimumHeight(32);
    connect(upBtn, &QPushButton::clicked, this, &SettingsPage::onMoveRepoUp);
    repoBtnRow->addWidget(upBtn);

    QPushButton* downBtn = new QPushButton(tr("下移"), repoGroup);
    downBtn->setMinimumHeight(32);
    connect(downBtn, &QPushButton::clicked, this, &SettingsPage::onMoveRepoDown);
    repoBtnRow->addWidget(downBtn);

    QPushButton* refreshBtn = new QPushButton(tr("立即刷新索引"), repoGroup);
    refreshBtn->setMinimumHeight(32);
    connect(refreshBtn, &QPushButton::clicked, this, &SettingsPage::onRefreshRepos);
    repoBtnRow->addWidget(refreshBtn);
    repoBtnRow->addStretch();
    repoLayout->addLayout(repoBtnRow);

    mainLayout->addWidget(repoGroup);

    mainLayout->addStretch();
}

void SettingsPage::loadSettings()
{
    // Block signals to avoid recursive calls
    m_languageCombo->blockSignals(true);
    m_behaviorCombo->blockSignals(true);
    m_themeCombo->blockSignals(true);
    m_indexIntervalCombo->blockSignals(true);
    m_indexSourceCombo->blockSignals(true);
    m_moduleSourceCombo->blockSignals(true);
    m_concurrencyCombo->blockSignals(true);
    m_installSuggestsToggle->blockSignals(true);

    QString lang = ConfigManager::instance().language();
    int langIdx = m_languageCombo->findData(lang);
    if (langIdx >= 0) m_languageCombo->setCurrentIndex(langIdx);

    int behavior = ConfigManager::instance().launchBehavior();
    int behIdx = m_behaviorCombo->findData(behavior);
    if (behIdx >= 0) m_behaviorCombo->setCurrentIndex(behIdx);

    QString theme = ConfigManager::instance().theme();
    int themeIdx = m_themeCombo->findData(theme);
    if (themeIdx >= 0) m_themeCombo->setCurrentIndex(themeIdx);

    int interval = ConfigManager::instance().indexRefreshIntervalSecs();
    int intIdx = m_indexIntervalCombo->findData(interval);
    if (intIdx >= 0) m_indexIntervalCombo->setCurrentIndex(intIdx);

    int indexSrc = static_cast<int>(ConfigManager::instance().indexDownloadSource());
    int idxIdx = m_indexSourceCombo->findData(indexSrc);
    if (idxIdx >= 0) m_indexSourceCombo->setCurrentIndex(idxIdx);

    int modSrc = static_cast<int>(ConfigManager::instance().moduleDownloadSource());
    int modIdx = m_moduleSourceCombo->findData(modSrc);
    if (modIdx >= 0) m_moduleSourceCombo->setCurrentIndex(modIdx);

    int concurrency = ConfigManager::instance().downloadConcurrency();
    int concIdx = m_concurrencyCombo->findData(concurrency);
    if (concIdx >= 0) m_concurrencyCombo->setCurrentIndex(concIdx);

    m_cacheDirLabel->setText(CKanManager::instance().downloadDir());

    m_installSuggestsToggle->setChecked(ConfigManager::instance().installSuggests());

    loadRepoList();

    m_languageCombo->blockSignals(false);
    m_behaviorCombo->blockSignals(false);
    m_themeCombo->blockSignals(false);
    m_indexIntervalCombo->blockSignals(false);
    m_indexSourceCombo->blockSignals(false);
    m_moduleSourceCombo->blockSignals(false);
    m_concurrencyCombo->blockSignals(false);
    m_installSuggestsToggle->blockSignals(false);
}

void SettingsPage::refreshBackgroundPreview()
{
    if (!m_backgroundPreviewLabel || !m_backgroundPathLabel) return;

    const QString stored = ConfigManager::instance().backgroundPath();
    if (stored.isEmpty()) {
        m_backgroundPathLabel->setText(tr("默认背景"));
    } else {
        m_backgroundPathLabel->setText(stored);
    }

    // 缩放预览图,保持比例
    const QPixmap& pix = BackgroundManager::instance().pixmap();
    if (pix.isNull()) {
        m_backgroundPreviewLabel->setText(tr("（无预览）"));
        m_backgroundPreviewLabel->setPixmap(QPixmap());
        return;
    }

    const QSize targetSize = m_backgroundPreviewLabel->size();
    if (targetSize.isEmpty()) {
        m_backgroundPreviewLabel->setPixmap(pix);
        return;
    }
    QPixmap scaled = pix.scaled(targetSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    // 居中裁剪
    if (scaled.size() != targetSize) {
        QPixmap cropped(scaled.size().boundedTo(targetSize));
        cropped.fill(Qt::transparent);
        // simpler: just use KeepAspectRatio
        scaled = pix.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    m_backgroundPreviewLabel->setPixmap(scaled);
    m_backgroundPreviewLabel->setText("");
}

void SettingsPage::onChooseBackgroundClicked()
{
    const QString startDir = QDir::homePath();
    const QString filePath = QFileDialog::getOpenFileName(this,
        tr("选择背景图片"),
        startDir,
        tr("图片文件 (*.png *.jpg *.jpeg);;PNG 图片 (*.png);;JPEG 图片 (*.jpg *.jpeg);;所有文件 (*.*)"));

    if (filePath.isEmpty()) {
        return;
    }

    bool ok = BackgroundManager::instance().setUserBackground(filePath);
    if (!ok) {
        QMessageBox::warning(this, tr("错误"),
            tr("无法应用所选图片。请确认文件是有效的 PNG/JPG/JPEG 格式，且未被占用。"));
    }
}

void SettingsPage::onResetBackgroundClicked()
{
    BackgroundManager::instance().resetToDefault();
}

void SettingsPage::onLanguageChanged(int index)
{
    QString lang = m_languageCombo->itemData(index).toString();
    ConfigManager::instance().setLanguage(lang);
    QMessageBox::information(this, tr("切换语言"),
        tr("语言切换将在重启启动器后生效。"));
}

void SettingsPage::onLaunchBehaviorChanged(int index)
{
    int behavior = m_behaviorCombo->itemData(index).toInt();
    ConfigManager::instance().setLaunchBehavior(static_cast<ConfigManager::LaunchBehavior>(behavior));
}

void SettingsPage::onThemeChanged(int index)
{
    QString theme = m_themeCombo->itemData(index).toString();
    ConfigManager::instance().setTheme(theme);
    emit themeChanged(theme);
}

void SettingsPage::onIndexIntervalChanged(int index)
{
    const int secs = m_indexIntervalCombo->itemData(index).toInt();
    ConfigManager::instance().setIndexRefreshIntervalSecs(secs);
}

void SettingsPage::onIndexSourceChanged(int index)
{
    const auto source = static_cast<ConfigManager::DownloadSource>(m_indexSourceCombo->itemData(index).toInt());
    ConfigManager::instance().setIndexDownloadSource(source);
}

void SettingsPage::onModuleSourceChanged(int index)
{
    const auto source = static_cast<ConfigManager::DownloadSource>(m_moduleSourceCombo->itemData(index).toInt());
    ConfigManager::instance().setModuleDownloadSource(source);
}

void SettingsPage::onConcurrencyChanged(int index)
{
    ConfigManager::instance().setDownloadConcurrency(m_concurrencyCombo->itemData(index).toInt());
}

void SettingsPage::onChooseCacheDirClicked()
{
    const QString current = CKanManager::instance().downloadDir();
    const QString startDir = QFileInfo(current).exists() ? current : QDir::homePath();
    const QString dir = QFileDialog::getExistingDirectory(this, tr("选择下载缓存文件夹"), startDir);
    if (dir.isEmpty() || QDir(dir).canonicalPath() == QDir(current).canonicalPath())
        return;

    // 询问是否迁移旧目录中的缓存文件
    const bool migrate = QMessageBox::question(this, tr("迁移缓存"),
        tr("是否将旧缓存目录中的文件移动到新目录？\n\n旧目录：%1\n新目录：%2")
            .arg(current, dir)) == QMessageBox::Yes;

    if (migrate) {
        QDir oldDir(current);
        QDir newDir(dir);
        if (oldDir.exists()) {
            newDir.mkpath(".");
            const QStringList files = oldDir.entryList(QDir::Files);
            for (const QString &f : files)
                QFile::rename(oldDir.filePath(f), newDir.filePath(f));
        }
    }

    ConfigManager::instance().setDownloadCacheDir(dir);
    m_cacheDirLabel->setText(CKanManager::instance().downloadDir());
}

void SettingsPage::onClearCacheClicked()
{
    const QString dir = CKanManager::instance().downloadDir();
    const auto choice = QMessageBox::question(this, tr("清理缓存"),
        tr("将删除下载缓存文件夹中的模组缓存文件（仅精确删除与已知模组对应的 .zip，不会误删其他文件）。\n\n目录：%1\n\n确定要清理吗？").arg(dir));
    if (choice != QMessageBox::Yes)
        return;

    const int removed = CKanManager::instance().cleanDownloadCache();
    if (removed > 0)
        QMessageBox::information(this, tr("清理完成"), tr("已清理 %1 个模组缓存文件。").arg(removed));
    else
        QMessageBox::information(this, tr("清理完成"), tr("没有可清理的模组缓存文件。"));
}

void SettingsPage::loadRepoList()
{
    const QVector<ckan::Repository> repos = ConfigManager::instance().repositories();
    m_repoList->clear();
    for (int i = 0; i < repos.size(); ++i) {
        const ckan::Repository &r = repos.at(i);
        QListWidgetItem *item = new QListWidgetItem(
            QStringLiteral("%1. %2").arg(i + 1).arg(r.name), m_repoList);
        item->setToolTip(r.uri);
        item->setData(Qt::UserRole, r.uri);
    }
    if (m_repoList->count() > 0)
        m_repoList->setCurrentRow(0);
}

void SettingsPage::applyRepos(const QVector<ckan::Repository> &repos, bool refresh)
{
    // 按 URI 去重，避免同一仓库重复添加
    QVector<ckan::Repository> dedup;
    QSet<QString> seen;
    for (const ckan::Repository &r : repos) {
        if (seen.contains(r.uri)) continue;
        seen.insert(r.uri);
        dedup.append(r);
    }
    ConfigManager::instance().setRepositories(dedup);
    loadRepoList();
    if (refresh)
        CKanManager::instance().refreshIndexAsync(true); // 仓库变更：强制重新下载索引
}

void SettingsPage::onAddPresetRepo(const ckan::Repository &preset)
{
    if (!preset.isValid()) return;
    QVector<ckan::Repository> repos = ConfigManager::instance().repositories();
    // 已存在相同 URI 时不再添加
    for (const ckan::Repository &r : repos) {
        if (r.uri == preset.uri) {
            QMessageBox::information(this, tr("仓库列表"), tr("该仓库已在列表中。"));
            return;
        }
    }
    repos.append(preset);
    applyRepos(repos);
}

void SettingsPage::onAddCustomRepo()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("自定义添加仓库"));
    dlg.setMinimumWidth(460);
    QVBoxLayout *lay = new QVBoxLayout(&dlg);

    QFormLayout *form = new QFormLayout();
    QLineEdit *nameEdit = new QLineEdit(&dlg);
    nameEdit->setPlaceholderText(tr("仓库名称（显示用，如 MyRepo）"));
    form->addRow(tr("名称："), nameEdit);
    QLineEdit *urlEdit = new QLineEdit(&dlg);
    urlEdit->setPlaceholderText(tr("https://.../CKAN-meta-xxx.tar.gz"));
    form->addRow(tr("地址："), urlEdit);
    lay->addLayout(form);

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(box);

    if (dlg.exec() != QDialog::Accepted) return;
    const QString name = nameEdit->text().trimmed();
    const QString url = urlEdit->text().trimmed();
    if (name.isEmpty() || url.isEmpty()) {
        QMessageBox::warning(this, tr("自定义添加仓库"), tr("名称和地址都不能为空。"));
        return;
    }
    ckan::Repository r;
    r.name = name;
    r.uri = url;
    onAddPresetRepo(r);
}

void SettingsPage::onRemoveRepo()
{
    const int row = m_repoList->currentRow();
    if (row < 0) return;
    QVector<ckan::Repository> repos = ConfigManager::instance().repositories();
    if (row >= repos.size()) return;
    const QString name = repos.at(row).name;
    if (QMessageBox::question(this, tr("删除仓库"),
            tr("确定删除仓库「%1」吗？").arg(name)) != QMessageBox::Yes)
        return;
    repos.removeAt(row);
    applyRepos(repos);
}

void SettingsPage::onMoveRepoUp()
{
    const int row = m_repoList->currentRow();
    if (row <= 0) return;
    QVector<ckan::Repository> repos = ConfigManager::instance().repositories();
    if (row >= repos.size()) return;
    repos.swapItemsAt(row, row - 1);
    applyRepos(repos);
    m_repoList->setCurrentRow(row - 1);
}

void SettingsPage::onMoveRepoDown()
{
    const int row = m_repoList->currentRow();
    QVector<ckan::Repository> repos = ConfigManager::instance().repositories();
    if (row < 0 || row >= repos.size() - 1) return;
    repos.swapItemsAt(row, row + 1);
    applyRepos(repos);
    m_repoList->setCurrentRow(row + 1);
}

void SettingsPage::onRefreshRepos()
{
    CKanManager::instance().refreshIndexAsync(true);
}

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
      m_cacheDirLabel(nullptr)
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

    m_cacheDirLabel->setText(CKanManager::instance().downloadDir());

    m_languageCombo->blockSignals(false);
    m_behaviorCombo->blockSignals(false);
    m_themeCombo->blockSignals(false);
    m_indexIntervalCombo->blockSignals(false);
    m_indexSourceCombo->blockSignals(false);
    m_moduleSourceCombo->blockSignals(false);
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

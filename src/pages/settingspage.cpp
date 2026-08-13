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
#include "../backgroundmanager.h"

SettingsPage::SettingsPage(QWidget *parent)
    : QWidget(parent),
      m_backgroundPathLabel(nullptr),
      m_backgroundPreviewLabel(nullptr),
      m_chooseBackgroundBtn(nullptr),
      m_resetBackgroundBtn(nullptr)
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
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
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

    mainLayout->addStretch();
}

void SettingsPage::loadSettings()
{
    // Block signals to avoid recursive calls
    m_languageCombo->blockSignals(true);
    m_behaviorCombo->blockSignals(true);
    m_themeCombo->blockSignals(true);

    QString lang = ConfigManager::instance().language();
    int langIdx = m_languageCombo->findData(lang);
    if (langIdx >= 0) m_languageCombo->setCurrentIndex(langIdx);

    int behavior = ConfigManager::instance().launchBehavior();
    int behIdx = m_behaviorCombo->findData(behavior);
    if (behIdx >= 0) m_behaviorCombo->setCurrentIndex(behIdx);

    QString theme = ConfigManager::instance().theme();
    int themeIdx = m_themeCombo->findData(theme);
    if (themeIdx >= 0) m_themeCombo->setCurrentIndex(themeIdx);

    m_languageCombo->blockSignals(false);
    m_behaviorCombo->blockSignals(false);
    m_themeCombo->blockSignals(false);
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

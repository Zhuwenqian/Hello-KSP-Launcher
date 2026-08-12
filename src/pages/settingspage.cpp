#include "settingspage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QFormLayout>
#include <QIcon>

SettingsPage::SettingsPage(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    loadSettings();
    connect(&ConfigManager::instance(), &ConfigManager::configChanged, this, &SettingsPage::loadSettings);
}

void SettingsPage::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 10, 20, 20);
    mainLayout->setSpacing(15);

    QLabel* titleLabel = new QLabel("启动器设置", this);
    titleLabel->setObjectName("pageTitle");
    mainLayout->addWidget(titleLabel);

    QGroupBox* generalGroup = new QGroupBox("通用设置", this);
    QFormLayout* generalLayout = new QFormLayout(generalGroup);
    generalLayout->setContentsMargins(20, 20, 20, 20);
    generalLayout->setSpacing(15);

    m_languageCombo = new QComboBox(generalGroup);
    m_languageCombo->addItem(QIcon(":/icons/globe.svg"), "简体中文", "zh_CN");
    m_languageCombo->addItem(QIcon(":/icons/globe.svg"), "English", "en_US");
    connect(m_languageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsPage::onLanguageChanged);

    QLabel* langLabel = new QLabel("启动器语言：", generalGroup);
    langLabel->setObjectName("settingLabel");
    generalLayout->addRow(langLabel, m_languageCombo);

    m_behaviorCombo = new QComboBox(generalGroup);
    m_behaviorCombo->addItem("保持窗口打开", static_cast<int>(ConfigManager::KeepOpen));
    m_behaviorCombo->addItem("最小化到任务栏", static_cast<int>(ConfigManager::Minimize));
    m_behaviorCombo->addItem("自动关闭启动器", static_cast<int>(ConfigManager::Close));
    connect(m_behaviorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsPage::onLaunchBehaviorChanged);

    QLabel* behaviorLabel = new QLabel("游戏启动后行为：", generalGroup);
    behaviorLabel->setObjectName("settingLabel");
    generalLayout->addRow(behaviorLabel, m_behaviorCombo);

    m_themeCombo = new QComboBox(generalGroup);
    m_themeCombo->addItem(QIcon(":/icons/moon.svg"), "深色主题", "dark");
    m_themeCombo->addItem(QIcon(":/icons/sun.svg"), "浅色主题", "light");
    connect(m_themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsPage::onThemeChanged);

    QLabel* themeLabel = new QLabel("主题：", generalGroup);
    themeLabel->setObjectName("settingLabel");
    generalLayout->addRow(themeLabel, m_themeCombo);

    mainLayout->addWidget(generalGroup);
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

void SettingsPage::onLanguageChanged(int index)
{
    QString lang = m_languageCombo->itemData(index).toString();
    ConfigManager::instance().setLanguage(lang);
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

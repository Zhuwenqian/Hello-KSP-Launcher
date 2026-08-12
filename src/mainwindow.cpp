#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QProcess>
#include <QIcon>
#include "instancemanager.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_gameRunning(false)
{
    setWindowTitle("Hello KSP Launcher");
    resize(1000, 650);
    setMinimumSize(800, 500);

    setupUI();
    applyTheme(ConfigManager::instance().theme());

    connect(&ConfigManager::instance(), &ConfigManager::currentInstanceChanged,
            this, &MainWindow::onCurrentInstanceChanged);
    connect(&InstanceManager::instance(), &InstanceManager::gameStarted,
            this, &MainWindow::onGameStarted);
    connect(&InstanceManager::instance(), &InstanceManager::gameFinished,
            this, &MainWindow::onGameFinished);
    connect(&InstanceManager::instance(), &InstanceManager::gameError,
            this, &MainWindow::onGameError);

    addTestInstanceIfEmpty();
    onCurrentInstanceChanged();

    // Default to home page
    setNavButtonChecked(m_homeBtn);
    showPage(m_homePage);
    m_homePage->refreshCurrentInstance();
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUI()
{
    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);

    QVBoxLayout* mainLayout = new QVBoxLayout(m_centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    QHBoxLayout* contentLayout = new QHBoxLayout();
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    setupSidebar();
    contentLayout->addWidget(m_sidebar);

    m_contentStack = new QStackedWidget(m_centralWidget);
    m_contentStack->setObjectName("contentStack");

    m_homePage = new HomePage(m_contentStack);
    m_instanceListPage = new InstanceListPage(m_contentStack);
    m_instanceDetailPage = new InstanceDetailPage(m_contentStack);
    m_settingsPage = new SettingsPage(m_contentStack);

    m_contentStack->addWidget(m_homePage);
    m_contentStack->addWidget(m_instanceListPage);
    m_contentStack->addWidget(m_instanceDetailPage);
    m_contentStack->addWidget(m_settingsPage);

    contentLayout->addWidget(m_contentStack, 1);
    mainLayout->addLayout(contentLayout, 1);

    setupLaunchBar();
    mainLayout->addWidget(m_launchBar);

    connect(m_instanceListPage, &InstanceListPage::instanceEntered,
            this, &MainWindow::onInstanceEntered);
    connect(m_instanceListPage, &InstanceListPage::addInstanceRequested,
            this, &MainWindow::onAddInstanceRequested);
    connect(m_instanceListPage, &InstanceListPage::currentInstanceChanged,
            this, &MainWindow::onCurrentInstanceChanged);
    connect(m_instanceListPage, &InstanceListPage::backClicked,
            this, &MainWindow::onBackToHome);
    connect(m_instanceDetailPage, &InstanceDetailPage::backClicked,
            this, &MainWindow::onBackToInstanceList);
    connect(m_settingsPage, &SettingsPage::themeChanged,
            this, &MainWindow::applyTheme);
}

void MainWindow::setupSidebar()
{
    m_sidebar = new QWidget(m_centralWidget);
    m_sidebar->setFixedWidth(220);
    m_sidebar->setObjectName("sidebarWidget");

    QVBoxLayout* sidebarLayout = new QVBoxLayout(m_sidebar);
    sidebarLayout->setContentsMargins(0, 0, 0, 0);
    sidebarLayout->setSpacing(0);

    QLabel* titleLabel = new QLabel("Hello KSP!", m_sidebar);
    titleLabel->setObjectName("sidebarTitle");
    sidebarLayout->addWidget(titleLabel);

    QLabel* gameSection = new QLabel("游戏", m_sidebar);
    gameSection->setObjectName("sidebarSectionLabel");
    sidebarLayout->addWidget(gameSection);

    m_homeBtn = new QPushButton(QIcon(":/icons/home.svg"), "  首页", m_sidebar);
    m_homeBtn->setObjectName("navButton");
    m_homeBtn->setCheckable(true);
    m_homeBtn->setMinimumHeight(45);
    connect(m_homeBtn, &QPushButton::clicked, this, &MainWindow::onNavButtonClicked);

    m_instanceManageBtn = new QPushButton(QIcon(":/icons/database.svg"), "  实例管理", m_sidebar);
    m_instanceManageBtn->setObjectName("navButton");
    m_instanceManageBtn->setCheckable(true);
    m_instanceManageBtn->setMinimumHeight(45);
    connect(m_instanceManageBtn, &QPushButton::clicked, this, &MainWindow::onNavButtonClicked);

    m_instanceListBtn = new QPushButton(QIcon(":/icons/list.svg"), "  实例列表", m_sidebar);
    m_instanceListBtn->setObjectName("navButton");
    m_instanceListBtn->setCheckable(true);
    m_instanceListBtn->setMinimumHeight(45);
    connect(m_instanceListBtn, &QPushButton::clicked, this, &MainWindow::onNavButtonClicked);

    sidebarLayout->addWidget(m_homeBtn);
    sidebarLayout->addWidget(m_instanceManageBtn);
    sidebarLayout->addWidget(m_instanceListBtn);

    QLabel* generalSection = new QLabel("通用", m_sidebar);
    generalSection->setObjectName("sidebarSectionLabel");
    sidebarLayout->addWidget(generalSection);

    m_settingsBtn = new QPushButton(QIcon(":/icons/settings.svg"), "  启动器设置", m_sidebar);
    m_settingsBtn->setObjectName("navButton");
    m_settingsBtn->setCheckable(true);
    m_settingsBtn->setMinimumHeight(45);
    connect(m_settingsBtn, &QPushButton::clicked, this, &MainWindow::onNavButtonClicked);

    sidebarLayout->addWidget(m_settingsBtn);
    sidebarLayout->addStretch();
}

void MainWindow::setupLaunchBar()
{
    m_launchBar = new QWidget(m_centralWidget);
    m_launchBar->setObjectName("launchBar");
    m_launchBar->setFixedHeight(70);

    QHBoxLayout* barLayout = new QHBoxLayout(m_launchBar);
    barLayout->setContentsMargins(20, 10, 20, 10);
    barLayout->setSpacing(10);

    m_currentInstanceLabel = new QLabel("未选择实例", m_launchBar);
    m_currentInstanceLabel->setObjectName("currentInstanceLabel");

    barLayout->addWidget(m_currentInstanceLabel, 1);

    m_launchSwitchButton = new QPushButton(QIcon(":/icons/chevron-down.svg"), "", m_launchBar);
    m_launchSwitchButton->setObjectName("launchSwitchButton");
    m_launchSwitchButton->setFixedSize(50, 50);
    m_launchSwitchButton->setToolTip("切换实例");
    connect(m_launchSwitchButton, &QPushButton::clicked, this, &MainWindow::onLaunchSwitchClicked);

    m_switchMenu = new QMenu(this);
    connect(m_switchMenu, &QMenu::triggered, this, &MainWindow::switchInstanceFromMenu);

    m_launchButton = new QPushButton(QIcon(":/icons/rocket.svg"), " 启动游戏", m_launchBar);
    m_launchButton->setObjectName("launchButton");
    m_launchButton->setMinimumHeight(50);
    m_launchButton->setMinimumWidth(180);
    connect(m_launchButton, &QPushButton::clicked, this, &MainWindow::onLaunchClicked);

    barLayout->addWidget(m_launchSwitchButton);
    barLayout->addWidget(m_launchButton);
}

void MainWindow::applyTheme(const QString &theme)
{
    m_currentTheme = theme;
    QString qssPath = ":/themes/" + theme + ".qss";
    QFile file(qssPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString styleSheet = file.readAll();
        setStyleSheet(styleSheet);
        file.close();
    }
}

void MainWindow::setNavButtonChecked(QPushButton* btn)
{
    m_homeBtn->setChecked(btn == m_homeBtn);
    m_instanceManageBtn->setChecked(btn == m_instanceManageBtn);
    m_instanceListBtn->setChecked(btn == m_instanceListBtn);
    m_settingsBtn->setChecked(btn == m_settingsBtn);
}

void MainWindow::onNavButtonClicked()
{
    QPushButton* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;

    setNavButtonChecked(btn);

    if (btn == m_homeBtn) {
        m_homePage->refreshCurrentInstance();
        showPage(m_homePage);
        m_launchBar->show();
    } else if (btn == m_instanceManageBtn) {
        KSPInstance cur = ConfigManager::instance().currentInstance();
        if (!cur.id.isEmpty()) {
            m_instanceDetailPage->loadCurrentInstance();
            showPage(m_instanceDetailPage);
            m_launchBar->hide();
        } else {
            QMessageBox::information(this, "提示", "请先选择或添加一个KSP实例");
            setNavButtonChecked(m_instanceListBtn);
            m_instanceListPage->refresh();
            showPage(m_instanceListPage);
            m_launchBar->hide();
        }
    } else if (btn == m_instanceListBtn) {
        m_instanceListPage->refresh();
        showPage(m_instanceListPage);
        m_launchBar->hide();
    } else if (btn == m_settingsBtn) {
        showPage(m_settingsPage);
        m_launchBar->hide();
    }
}

void MainWindow::onAddInstanceRequested()
{
    QString exePath = QFileDialog::getOpenFileName(this,
        "选择KSP可执行文件",
        QString(),
        "KSP可执行文件 (KSP*.exe);;所有文件 (*.*)");

    if (exePath.isEmpty()) return;

    QString rootPath = InstanceManager::instance().detectGameRoot(exePath);
    if (!InstanceManager::instance().isValidKSPPath(rootPath)) {
        QMessageBox::warning(this, "错误", "所选目录不是有效的KSP游戏目录，请确认包含settings.cfg和GameData文件夹。");
        return;
    }

    QFileInfo fi(exePath);
    KSPInstance inst;
    inst.name = fi.baseName();
    inst.path = rootPath;
    inst.exePath = exePath;
    ConfigManager::instance().addInstance(inst);

    onCurrentInstanceChanged();
}

void MainWindow::onInstanceEntered(const QString &id)
{
    setNavButtonChecked(m_instanceManageBtn);
    m_instanceDetailPage->setInstanceId(id);
    showPage(m_instanceDetailPage);
    m_launchBar->hide();
}

void MainWindow::onBackToInstanceList()
{
    setNavButtonChecked(m_instanceListBtn);
    m_instanceListPage->refresh();
    showPage(m_instanceListPage);
    m_launchBar->hide();
}

void MainWindow::onBackToHome()
{
    setNavButtonChecked(m_homeBtn);
    m_homePage->refreshCurrentInstance();
    showPage(m_homePage);
    m_launchBar->show();
}

void MainWindow::showPage(QWidget *page)
{
    m_contentStack->setCurrentWidget(page);
}

void MainWindow::onLaunchClicked()
{
    if (m_gameRunning) {
        QMessageBox::information(this, "提示", "游戏已经在运行中。");
        return;
    }

    KSPInstance inst = ConfigManager::instance().currentInstance();
    if (inst.id.isEmpty()) {
        QMessageBox::warning(this, "错误", "请先选择一个KSP实例。");
        return;
    }

    if (!QFile::exists(inst.exePath)) {
        QMessageBox::warning(this, "错误", "找不到游戏可执行文件，请检查实例路径。");
        return;
    }

    bool launched = InstanceManager::instance().launchGame(inst.exePath);
    if (!launched) {
        QMessageBox::warning(this, "错误", "启动游戏失败。");
    }
}

void MainWindow::onLaunchSwitchClicked()
{
    m_switchMenu->clear();
    QList<KSPInstance> instances = ConfigManager::instance().instances();
    KSPInstance current = ConfigManager::instance().currentInstance();

    for (const KSPInstance& inst : instances) {
        QAction* action = m_switchMenu->addAction(inst.name);
        action->setData(inst.id);
        if (inst.id == current.id) {
            action->setCheckable(true);
            action->setChecked(true);
        }
    }

    if (instances.isEmpty()) {
        m_switchMenu->addAction("（无可用实例）")->setEnabled(false);
    }

    QPoint pos = m_launchSwitchButton->mapToGlobal(
        QPoint(0, -m_switchMenu->sizeHint().height()));
    m_switchMenu->exec(pos);
}

void MainWindow::switchInstanceFromMenu(QAction *action)
{
    QString id = action->data().toString();
    if (!id.isEmpty()) {
        ConfigManager::instance().setCurrentInstance(id);
        onCurrentInstanceChanged();
        m_homePage->refreshCurrentInstance();
    }
}

void MainWindow::onGameStarted()
{
    m_gameRunning = true;
    m_launchButton->setIcon(QIcon(":/icons/stop.svg"));
    m_launchButton->setText(" 游戏运行中");
    m_launchButton->setEnabled(false);

    ConfigManager::LaunchBehavior behavior = ConfigManager::instance().launchBehavior();
    switch (behavior) {
    case ConfigManager::Minimize:
        showMinimized();
        break;
    case ConfigManager::Close:
        showMinimized();
        break;
    case ConfigManager::KeepOpen:
    default:
        break;
    }
}

void MainWindow::onGameFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(exitCode);
    Q_UNUSED(status);
    m_gameRunning = false;
    m_launchButton->setIcon(QIcon(":/icons/rocket.svg"));
    m_launchButton->setText(" 启动游戏");
    m_launchButton->setEnabled(true);

    if (ConfigManager::instance().launchBehavior() == ConfigManager::Minimize) {
        showNormal();
        activateWindow();
    }
}

void MainWindow::onGameError(QProcess::ProcessError error)
{
    Q_UNUSED(error);
    m_gameRunning = false;
    m_launchButton->setIcon(QIcon(":/icons/rocket.svg"));
    m_launchButton->setText(" 启动游戏");
    m_launchButton->setEnabled(true);
    showNormal();
    activateWindow();
    QMessageBox::warning(this, "错误", "游戏进程发生错误。");
}

void MainWindow::onCurrentInstanceChanged()
{
    KSPInstance cur = ConfigManager::instance().currentInstance();
    if (cur.id.isEmpty()) {
        m_currentInstanceLabel->setText("未选择实例");
        m_launchButton->setEnabled(false);
        m_instanceManageBtn->setEnabled(false);
    } else {
        m_currentInstanceLabel->setText(cur.name);
        m_launchButton->setEnabled(!m_gameRunning);
        m_instanceManageBtn->setEnabled(true);
    }
    m_instanceListPage->refresh();
}

void MainWindow::addTestInstanceIfEmpty()
{
    QList<KSPInstance> instances = ConfigManager::instance().instances();
    QString testPath = "D:/Game/KSP";
    QString testExe = "D:/Game/KSP/KSP_x64.exe";

    if (instances.isEmpty() &&
        InstanceManager::instance().isValidKSPPath(testPath) &&
        QFile::exists(testExe)) {
        KSPInstance inst;
        inst.name = "KSP 测试实例";
        inst.path = testPath;
        inst.exePath = testExe;
        ConfigManager::instance().addInstance(inst);
    }
}

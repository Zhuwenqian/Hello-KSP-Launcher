#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QProcess>
#include <QIcon>
#include <QPalette>
#include <QResizeEvent>
#include <QTimer>
#include "instancemanager.h"
#include "iconutils.h"
#include "backgroundmanager.h"
#include "steamdiscovery.h"
#include "ckan/ckan.h"
#include <QSet>
#include <QFileInfo>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_gameRunning(false),
      m_backgroundLabel(nullptr), m_contentContainer(nullptr)
{
    setWindowTitle("Hello KSP Launcher");
    resize(1000, 650);
    setMinimumSize(800, 500);

    setupUI();
    applyTheme(ConfigManager::instance().theme());
    applyTransparency();

    // 初始化背景(在 UI 搭好之后,保证第一次 update 能命中尺寸)
    BackgroundManager::instance().initialize();
    updateBackgroundPixmap();
    connect(&BackgroundManager::instance(), &BackgroundManager::backgroundChanged,
            this, &MainWindow::onBackgroundChanged);

    connect(&ConfigManager::instance(), &ConfigManager::currentInstanceChanged,
            this, &MainWindow::onCurrentInstanceChanged);
    connect(&InstanceManager::instance(), &InstanceManager::gameStarted,
            this, &MainWindow::onGameStarted);
    connect(&InstanceManager::instance(), &InstanceManager::gameFinished,
            this, &MainWindow::onGameFinished);
    connect(&InstanceManager::instance(), &InstanceManager::gameError,
            this, &MainWindow::onGameError);

    onCurrentInstanceChanged();

    // Default to home page
    setNavButtonChecked(m_homeBtn);
    showPage(m_homePage);
    m_homePage->refreshCurrentInstance();

    // 启动时自动扫描 Steam 库，把发现的 KSP 直接加入实例列表（窗口显示后执行）
    QTimer::singleShot(0, this, &MainWindow::runSteamDiscovery);
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUI()
{
    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);

    // 1. 底层背景 QLabel:放在 centralWidget 中,不使用布局,手动 setGeometry 与窗口同步
    m_backgroundLabel = new QLabel(m_centralWidget);
    m_backgroundLabel->setObjectName("backgroundLabel");
    m_backgroundLabel->setScaledContents(true);
    m_backgroundLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_backgroundLabel->setAttribute(Qt::WA_NoSystemBackground, true);
    m_backgroundLabel->lower();

    // 2. 内容容器:包裹 sidebar + content + launchBar,
    //    通过 m_contentContainer 把整体背景设为 transparent,让背景图透出
    m_contentContainer = new QWidget(m_centralWidget);
    m_contentContainer->setObjectName("contentContainer");
    m_contentContainer->setAttribute(Qt::WA_TranslucentBackground, true);

    QVBoxLayout* mainLayout = new QVBoxLayout(m_contentContainer);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    QHBoxLayout* contentLayout = new QHBoxLayout();
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    setupSidebar();
    contentLayout->addWidget(m_sidebar);

    m_contentStack = new QStackedWidget(m_contentContainer);
    m_contentStack->setObjectName("contentStack");

    m_homePage = new HomePage(m_contentStack);
    m_instanceListPage = new InstanceListPage(m_contentStack);
    m_instanceDetailPage = new InstanceDetailPage(m_contentStack);
    m_settingsPage = new SettingsPage(m_contentStack);
    m_savesListPage = new SavesListPage(m_contentStack);
    m_saveDetailPage = new SaveDetailPage(m_contentStack);

    m_contentStack->addWidget(m_homePage);
    m_contentStack->addWidget(m_instanceListPage);
    m_contentStack->addWidget(m_instanceDetailPage);
    m_contentStack->addWidget(m_settingsPage);
    m_contentStack->addWidget(m_savesListPage);
    m_contentStack->addWidget(m_saveDetailPage);

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
    connect(m_instanceDetailPage, &InstanceDetailPage::savesManageRequested,
            this, &MainWindow::onSavesManageRequested);
    connect(m_settingsPage, &SettingsPage::themeChanged,
            this, &MainWindow::applyTheme);
    connect(m_savesListPage, &SavesListPage::backClicked,
            this, &MainWindow::onBackFromSavesList);
    connect(m_savesListPage, &SavesListPage::saveSelected,
            this, &MainWindow::onSaveSelected);
    connect(m_saveDetailPage, &SaveDetailPage::backClicked,
            this, &MainWindow::onBackFromSaveDetail);
    connect(m_saveDetailPage, &SaveDetailPage::homeClicked,
            this, &MainWindow::onHomeFromSaveDetail);

    // 初始化 contentContainer 铺满 centralWidget(首次 show 前)
    QTimer::singleShot(0, this, [this] {
        if (m_contentContainer && m_centralWidget) {
            m_contentContainer->setGeometry(m_centralWidget->rect());
        }
    });
}

void MainWindow::setupSidebar()
{
    m_sidebar = new QWidget(m_contentContainer);
    m_sidebar->setFixedWidth(220);
    m_sidebar->setObjectName("sidebarWidget");

    QVBoxLayout* sidebarLayout = new QVBoxLayout(m_sidebar);
    sidebarLayout->setContentsMargins(0, 0, 0, 0);
    sidebarLayout->setSpacing(0);

    

    QLabel* gameSection = new QLabel(tr("游戏"), m_sidebar);
    gameSection->setObjectName("sidebarSectionLabel");
    sidebarLayout->addWidget(gameSection);

    m_homeBtn = new QPushButton(IconUtils::tintedIcon(":/icons/home.svg", "#ffffff"), tr("  首页"), m_sidebar);
    m_homeBtn->setObjectName("navButton");
    m_homeBtn->setCheckable(true);
    m_homeBtn->setMinimumHeight(45);
    connect(m_homeBtn, &QPushButton::clicked, this, &MainWindow::onNavButtonClicked);

    m_instanceManageBtn = new QPushButton(IconUtils::tintedIcon(":/icons/database.svg", "#ffffff"), tr("  实例管理"), m_sidebar);
    m_instanceManageBtn->setObjectName("navButton");
    m_instanceManageBtn->setCheckable(true);
    m_instanceManageBtn->setMinimumHeight(45);
    connect(m_instanceManageBtn, &QPushButton::clicked, this, &MainWindow::onNavButtonClicked);

    m_instanceListBtn = new QPushButton(IconUtils::tintedIcon(":/icons/list.svg", "#ffffff"), tr("  实例列表"), m_sidebar);
    m_instanceListBtn->setObjectName("navButton");
    m_instanceListBtn->setCheckable(true);
    m_instanceListBtn->setMinimumHeight(45);
    connect(m_instanceListBtn, &QPushButton::clicked, this, &MainWindow::onNavButtonClicked);

    sidebarLayout->addWidget(m_homeBtn);
    sidebarLayout->addWidget(m_instanceManageBtn);
    sidebarLayout->addWidget(m_instanceListBtn);

    QLabel* generalSection = new QLabel(tr("通用"), m_sidebar);
    generalSection->setObjectName("sidebarSectionLabel");
    sidebarLayout->addWidget(generalSection);

    m_settingsBtn = new QPushButton(IconUtils::tintedIcon(":/icons/settings.svg", "#ffffff"), tr("  启动器设置"), m_sidebar);
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

    m_currentInstanceLabel = new QLabel(tr("未选择实例"), m_launchBar);
    m_currentInstanceLabel->setObjectName("currentInstanceLabel");

    barLayout->addWidget(m_currentInstanceLabel, 1);

    m_launchSwitchButton = new QPushButton(IconUtils::tintedIcon(":/icons/chevron-down.svg", "#ffffff"), "", m_launchBar);
    m_launchSwitchButton->setObjectName("launchSwitchButton");
    m_launchSwitchButton->setFixedSize(50, 50);
    m_launchSwitchButton->setToolTip(tr("切换实例"));
    connect(m_launchSwitchButton, &QPushButton::clicked, this, &MainWindow::onLaunchSwitchClicked);

    m_switchMenu = new QMenu(this);
    connect(m_switchMenu, &QMenu::triggered, this, &MainWindow::switchInstanceFromMenu);

    m_launchButton = new QPushButton(IconUtils::tintedIcon(":/icons/rocket.svg", "#ffffff"), tr(" 启动游戏"), m_launchBar);
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
    applyTransparency();
    refreshIcons(theme);
}

void MainWindow::applyTransparency()
{
    if (!m_centralWidget) return;
    m_centralWidget->setAutoFillBackground(false);
    if (m_contentContainer) {
        m_contentContainer->setAttribute(Qt::WA_TranslucentBackground, true);
        // 确保始终铺满
        m_contentContainer->setGeometry(m_centralWidget->rect());
        m_contentContainer->raise();
    }
    if (m_backgroundLabel) {
        m_backgroundLabel->setGeometry(m_centralWidget->rect());
        m_backgroundLabel->lower();
    }
}

void MainWindow::updateBackgroundPixmap()
{
    if (!m_backgroundLabel) return;
    const QPixmap& pix = BackgroundManager::instance().pixmap();
    if (pix.isNull()) {
        m_backgroundLabel->clear();
        return;
    }
    // Cover 模式:按 centralWidget 尺寸缩放填充,保持比例,超出裁剪
    const QSize target = m_centralWidget ? m_centralWidget->size() : size();
    if (target.isEmpty()) {
        m_backgroundLabel->setPixmap(pix);
        return;
    }
    m_backgroundLabel->setPixmap(
        pix.scaled(target, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
}

void MainWindow::onBackgroundChanged()
{
    updateBackgroundPixmap();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (!m_centralWidget) return;
    const QRect r = m_centralWidget->rect();
    // 背景图和内容容器都跟随 centralWidget 尺寸,保证 launch bar 贴底
    if (m_backgroundLabel) {
        m_backgroundLabel->setGeometry(r);
        updateBackgroundPixmap();
    }
    if (m_contentContainer) {
        m_contentContainer->setGeometry(r);
    }
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    // 切换主题后 Qt 可能重新填充背景,这里确保背景 QLabel 仍在最底层
    if (event->type() == QEvent::StyleChange || event->type() == QEvent::PaletteChange) {
        if (m_backgroundLabel) {
            m_backgroundLabel->lower();
        }
    }
}

void MainWindow::refreshIcons(const QString &theme)
{
    QString color = IconUtils::iconColorForTheme(theme);

    m_homeBtn->setIcon(IconUtils::tintedIcon(":/icons/home.svg", color));
    m_instanceManageBtn->setIcon(IconUtils::tintedIcon(":/icons/database.svg", color));
    m_instanceListBtn->setIcon(IconUtils::tintedIcon(":/icons/list.svg", color));
    m_settingsBtn->setIcon(IconUtils::tintedIcon(":/icons/settings.svg", color));
    m_launchSwitchButton->setIcon(IconUtils::tintedIcon(":/icons/chevron-down.svg", color));

    if (m_gameRunning) {
        m_launchButton->setIcon(IconUtils::tintedIcon(":/icons/stop.svg", color));
    } else {
        m_launchButton->setIcon(IconUtils::tintedIcon(":/icons/rocket.svg", color));
    }

    m_instanceListPage->refreshIcons(color);
    m_instanceDetailPage->refreshIcons(color);
    m_savesListPage->refreshIcons(color);
    m_saveDetailPage->refreshIcons(color);
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
            QMessageBox::information(this, tr("提示"), tr("请先选择或添加一个KSP实例"));
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
        tr("选择KSP可执行文件"),
        QString(),
        tr("KSP可执行文件 (KSP*.exe);;所有文件 (*.*)"));

    if (exePath.isEmpty()) return;

    QString rootPath = InstanceManager::instance().detectGameRoot(exePath);
    if (!InstanceManager::instance().isValidKSPPath(rootPath)) {
        QMessageBox::warning(this, tr("错误"), tr("所选目录不是有效的KSP游戏目录，请确认包含settings.cfg和GameData文件夹。"));
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

void MainWindow::runSteamDiscovery()
{
#ifdef Q_OS_WIN
    const QStringList kspDirs = SteamDiscovery::discoverKSPDirs();
    if (kspDirs.isEmpty()) return;

    // 已存在实例按规范化路径去重，避免重复加入
    QSet<QString> existingPaths;
    const QList<KSPInstance> instances = ConfigManager::instance().instances();
    for (const KSPInstance &inst : instances)
        existingPaths.insert(QDir::cleanPath(inst.path));

    int added = 0;
    for (const QString &dir : kspDirs) {
        const QString cleanDir = QDir::cleanPath(dir);
        if (existingPaths.contains(cleanDir)) continue;

        KSPInstance inst;
        inst.path = cleanDir;
        // Windows 上优先 KSP_x64.exe，回退 KSP.exe
        const QString exe64 = QDir(cleanDir).filePath(QStringLiteral("KSP_x64.exe"));
        const QString exe32 = QDir(cleanDir).filePath(QStringLiteral("KSP.exe"));
        inst.exePath = QFile::exists(exe64) ? exe64 : exe32;

        // 名称 = 目录名 + 检测到的版本号（如 "Kerbal Space Program (1.12.5)"）
        inst.name = QFileInfo(cleanDir).fileName();
        const ckan::GameVersion version = ckan::CKan::detectVersionFromDir(cleanDir);
        if (version.isValid())
            inst.name += QStringLiteral(" (%1)").arg(version.toString());

        // 仅加入实例列表，不切换当前实例（实例列表页经 instancesChanged 自动刷新）
        ConfigManager::instance().addInstance(inst);
        existingPaths.insert(cleanDir);
        ++added;
    }

    if (added > 0) {
        qInfo().noquote() << QStringLiteral("Steam 发现：自动加入 %1 个 KSP 实例").arg(added);
        onCurrentInstanceChanged();
    }
#endif
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
        QMessageBox::information(this, tr("提示"), tr("游戏已经在运行中。"));
        return;
    }

    KSPInstance inst = ConfigManager::instance().currentInstance();
    if (inst.id.isEmpty()) {
        QMessageBox::warning(this, tr("错误"), tr("请先选择一个KSP实例。"));
        return;
    }

    if (!QFile::exists(inst.exePath)) {
        QMessageBox::warning(this, tr("错误"), tr("找不到游戏可执行文件，请检查实例路径。"));
        return;
    }

    bool launched = InstanceManager::instance().launchGame(inst.exePath, inst.launchArgs);
    if (!launched) {
        QMessageBox::warning(this, tr("错误"), tr("启动游戏失败。"));
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
        m_switchMenu->addAction(tr("（无可用实例）"))->setEnabled(false);
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
    m_launchButton->setIcon(IconUtils::tintedIcon(":/icons/stop.svg", IconUtils::iconColorForTheme(m_currentTheme)));
    m_launchButton->setText(tr(" 游戏运行中"));
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
    m_launchButton->setIcon(IconUtils::tintedIcon(":/icons/rocket.svg", IconUtils::iconColorForTheme(m_currentTheme)));
    m_launchButton->setText(tr(" 启动游戏"));
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
    m_launchButton->setIcon(IconUtils::tintedIcon(":/icons/rocket.svg", IconUtils::iconColorForTheme(m_currentTheme)));
    m_launchButton->setText(tr(" 启动游戏"));
    m_launchButton->setEnabled(true);
    showNormal();
    activateWindow();
    QMessageBox::warning(this, tr("错误"), tr("游戏进程发生错误。"));
}

void MainWindow::onCurrentInstanceChanged()
{
    KSPInstance cur = ConfigManager::instance().currentInstance();
    if (cur.id.isEmpty()) {
        m_currentInstanceLabel->setText(tr("未选择实例"));
        m_launchButton->setEnabled(false);
        m_instanceManageBtn->setEnabled(false);
    } else {
        m_currentInstanceLabel->setText(cur.name);
        m_launchButton->setEnabled(!m_gameRunning);
        m_instanceManageBtn->setEnabled(true);
    }
    m_instanceListPage->refresh();
}

void MainWindow::onSavesManageRequested()
{
    KSPInstance cur = ConfigManager::instance().currentInstance();
    if (cur.id.isEmpty()) {
        QMessageBox::information(this, "提示", "请先选择一个KSP实例");
        return;
    }
    m_savesListPage->setInstanceId(cur.id);
    showPage(m_savesListPage);
    m_launchBar->hide();
    // 取消侧边栏按钮高亮
    setNavButtonChecked(nullptr);
}

void MainWindow::onBackFromSavesList()
{
    // 返回实例详情页
    KSPInstance cur = ConfigManager::instance().currentInstance();
    if (!cur.id.isEmpty()) {
        m_instanceDetailPage->loadCurrentInstance();
    }
    setNavButtonChecked(m_instanceManageBtn);
    showPage(m_instanceDetailPage);
    m_launchBar->hide();
}

void MainWindow::onSaveSelected(const QString &savePath)
{
    m_saveDetailPage->setSavePath(savePath);
    showPage(m_saveDetailPage);
    m_launchBar->hide();
    setNavButtonChecked(nullptr);
}

void MainWindow::onBackFromSaveDetail()
{
    KSPInstance cur = ConfigManager::instance().currentInstance();
    if (!cur.id.isEmpty()) {
        m_savesListPage->setInstanceId(cur.id);
    }
    showPage(m_savesListPage);
    m_launchBar->hide();
    setNavButtonChecked(nullptr);
}

void MainWindow::onHomeFromSaveDetail()
{
    setNavButtonChecked(m_homeBtn);
    m_homePage->refreshCurrentInstance();
    showPage(m_homePage);
    m_launchBar->show();
}

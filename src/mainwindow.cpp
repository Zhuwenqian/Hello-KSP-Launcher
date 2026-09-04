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
#include "updateflow.h"
#include "ckan/ckan.h"
#include "ckan/gameinstance.h"
#include <QSet>
#include <QFileInfo>
#include <QMouseEvent>
#include <QShowEvent>
#include <QWindow>
#include <QApplication>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>

// 较旧的 mingw SDK 头缺失圆角窗口相关常量，这里补充定义
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#ifndef DWMWCP_DONOTROUND
enum DWM_WINDOW_CORNER_PREFERENCE {
    DWMWCP_DEFAULT = 0,
    DWMWCP_DONOTROUND = 1,
    DWMWCP_ROUND = 2,
    DWMWCP_ROUNDSMALL = 3
};
#endif
#endif
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_gameRunning(false),
      m_backgroundLabel(nullptr), m_contentContainer(nullptr), m_stoppingGame(false),
      m_titleBar(nullptr), m_winMinBtn(nullptr), m_winMaxBtn(nullptr), m_winCloseBtn(nullptr),
      m_dwmApplied(false), m_titleBarDrag(false)
{
    // 无边框窗口 + 自绘标题栏，实现主题色半透明标题栏
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
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

    // 延迟静默检查更新（窗口就绪后再发起，避免冷启动阻塞）
    QTimer::singleShot(8000, this, []() { updateflow::checkSilent(); });
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

    setupTitleBar();
    mainLayout->insertWidget(0, m_titleBar);

    setupSidebar();
    contentLayout->addWidget(m_sidebar);

    m_contentStack = new QStackedWidget(m_contentContainer);
    m_contentStack->setObjectName("contentStack");

    m_homePage = new HomePage(m_contentStack);
    m_instanceListPage = new InstanceListPage(m_contentStack);
    m_instanceDetailPage = new InstanceDetailPage(m_contentStack);
    m_settingsPage = new SettingsPage(m_contentStack);
    m_aboutPage = new AboutPage(m_contentStack);
    m_savesListPage = new SavesListPage(m_contentStack);
    m_saveDetailPage = new SaveDetailPage(m_contentStack);

    m_contentStack->addWidget(m_homePage);
    m_contentStack->addWidget(m_instanceListPage);
    m_contentStack->addWidget(m_instanceDetailPage);
    m_contentStack->addWidget(m_settingsPage);
    m_contentStack->addWidget(m_aboutPage);
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
    // 存档管理页的实例二级菜单→跳回详情页对应 tab
    connect(m_savesListPage, &SavesListPage::navToDetail,
            this, &MainWindow::onSavesNavToDetail);
    connect(m_savesListPage, &SavesListPage::modpackActionRequested,
            this, &MainWindow::onSavesModpackAction);
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

    m_aboutBtn = new QPushButton(IconUtils::tintedIcon(":/icons/info.svg", "#ffffff"), tr("  关于"), m_sidebar);
    m_aboutBtn->setObjectName("navButton");
    m_aboutBtn->setCheckable(true);
    m_aboutBtn->setMinimumHeight(45);
    connect(m_aboutBtn, &QPushButton::clicked, this, &MainWindow::onNavButtonClicked);

    sidebarLayout->addWidget(m_aboutBtn);
    sidebarLayout->addStretch();
}

void MainWindow::setupTitleBar()
{
    m_titleBar = new QWidget(m_contentContainer);
    m_titleBar->setObjectName("titleBar");
    m_titleBar->setFixedHeight(38);
    m_titleBar->installEventFilter(this);

    QHBoxLayout* tbLayout = new QHBoxLayout(m_titleBar);
    tbLayout->setContentsMargins(14, 0, 0, 0);
    tbLayout->setSpacing(8);

    QLabel* appIcon = new QLabel(m_titleBar);
    appIcon->setPixmap(QIcon(QStringLiteral(":/appicon.ico")).pixmap(18, 18));
    tbLayout->addWidget(appIcon);

    m_titleLabel = new QLabel(tr("Hello KSP Launcher"), m_titleBar);
    m_titleLabel->setObjectName("titleLabel");
    tbLayout->addWidget(m_titleLabel);
    tbLayout->addStretch();

    auto makeWinBtn = [&](const QString &iconPath) {
        QPushButton* b = new QPushButton(IconUtils::tintedIcon(iconPath, "#d0d0d0"), QString(), m_titleBar);
        b->setObjectName("winBtn");
        b->setFixedSize(46, 38);
        b->setCursor(Qt::ArrowCursor);
        return b;
    };

    m_winMinBtn = makeWinBtn(QStringLiteral(":/icons/window-minimize.svg"));
    m_winMaxBtn = makeWinBtn(QStringLiteral(":/icons/window-maximize.svg"));
    m_winCloseBtn = makeWinBtn(QStringLiteral(":/icons/window-close.svg"));
    m_winCloseBtn->setObjectName("winCloseBtn");

    connect(m_winMinBtn, &QPushButton::clicked, this, &MainWindow::onWindowMinClicked);
    connect(m_winMaxBtn, &QPushButton::clicked, this, &MainWindow::onWindowMaxClicked);
    connect(m_winCloseBtn, &QPushButton::clicked, this, &MainWindow::onWindowCloseClicked);

    tbLayout->addWidget(m_winMinBtn);
    tbLayout->addWidget(m_winMaxBtn);
    tbLayout->addWidget(m_winCloseBtn);
}

void MainWindow::onWindowMinClicked()
{
    showMinimized();
}

void MainWindow::onWindowMaxClicked()
{
    toggleMaximize();
}

void MainWindow::onWindowCloseClicked()
{
    close();
}

void MainWindow::toggleMaximize()
{
    if (isMaximized())
        showNormal();
    else
        showMaximized();
    updateWindowButtons();
    applyWindowCornerPreference();
}

void MainWindow::updateWindowButtons()
{
    if (!m_winMaxBtn) return;
    const QString icon = isMaximized()
        ? QStringLiteral(":/icons/window-restore.svg")
        : QStringLiteral(":/icons/window-maximize.svg");
    m_winMaxBtn->setIcon(IconUtils::tintedIcon(icon, "#d0d0d0"));
}

bool MainWindow::eventFilter(QObject *obj, QEvent *ev)
{
    if (obj == m_titleBar) {
        if (ev->type() == QEvent::MouseButtonPress) {
            QMouseEvent* me = static_cast<QMouseEvent*>(ev);
            if (me->button() == Qt::LeftButton && windowHandle()) {
                m_titleBarDrag = true;
                m_titleBarPressPos = me->globalPosition().toPoint();
                // 不消费事件：让系统有机会判定后续双击
                return false;
            }
        } else if (ev->type() == QEvent::MouseButtonRelease) {
            m_titleBarDrag = false;
        } else if (ev->type() == QEvent::MouseMove) {
            // 拖动超过阈值才交给系统 move，避免和双击最大化冲突；支持 Aero 贴靠
            if (m_titleBarDrag && windowHandle()) {
                const QPoint cur = static_cast<QMouseEvent*>(ev)->globalPosition().toPoint();
                if ((cur - m_titleBarPressPos).manhattanLength() >= QApplication::startDragDistance()) {
                    m_titleBarDrag = false;
                    windowHandle()->startSystemMove();
                    return true;
                }
            }
        } else if (ev->type() == QEvent::MouseButtonDblClick) {
            QMouseEvent* me = static_cast<QMouseEvent*>(ev);
            if (me->button() == Qt::LeftButton) {
                m_titleBarDrag = false;
                toggleMaximize();
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(obj, ev);
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    if (!m_dwmApplied) {
        m_dwmApplied = true;
        updateWindowButtons();
        QTimer::singleShot(0, this, &MainWindow::applyWindowCornerPreference);
    }
}

void MainWindow::applyWindowCornerPreference()
{
#if defined(_WIN32)
    if (!windowHandle()) return;
    const HWND hwnd = reinterpret_cast<HWND>(windowHandle()->winId());
    // 最大化时直角，普通状态圆角
    DWM_WINDOW_CORNER_PREFERENCE pref = isMaximized() ? DWMWCP_DONOTROUND : DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));
#endif
}

#if defined(_WIN32)
bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    Q_UNUSED(eventType);
    MSG* msg = static_cast<MSG*>(message);
    // 无边框窗口仅在普通(非最大化/全屏)状态处理边缘缩放
    if (msg->message == WM_NCHITTEST && !isMaximized() && !isFullScreen()) {
        const QPoint pos = mapFromGlobal(QCursor::pos());
        const QRect r = rect();
        const int b = 6;
        const bool top    = pos.y() <= b;
        const bool bottom = pos.y() >= r.height() - 1 - b;
        const bool left   = pos.x() <= b;
        const bool right  = pos.x() >= r.width() - 1 - b;

        int ht = HTCLIENT;
        if      (top && left)  ht = HTTOPLEFT;
        else if (top && right) ht = HTTOPRIGHT;
        else if (bottom && left)  ht = HTBOTTOMLEFT;
        else if (bottom && right) ht = HTBOTTOMRIGHT;
        else if (top)    ht = HTTOP;
        else if (bottom) ht = HTBOTTOM;
        else if (left)   ht = HTLEFT;
        else if (right)  ht = HTRIGHT;
        *result = ht;
        return true;
    }
    return QMainWindow::nativeEvent(eventType, message, result);
}
#endif

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
    m_aboutBtn->setIcon(IconUtils::tintedIcon(":/icons/info.svg", color));
    m_launchSwitchButton->setIcon(IconUtils::tintedIcon(":/icons/chevron-down.svg", color));

    if (m_gameRunning) {
        m_launchButton->setIcon(IconUtils::tintedIcon(":/icons/stop.svg", "#ffffff"));
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
    m_aboutBtn->setChecked(btn == m_aboutBtn);
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
    } else if (btn == m_aboutBtn) {
        showPage(m_aboutPage);
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
        QMessageBox::warning(this, tr("错误"), tr("所选目录不是有效的KSP游戏目录，请确认包含KSP可执行文件和GameData文件夹。"));
        return;
    }

    QFileInfo fi(exePath);
    KSPInstance inst;
    inst.exePath = exePath;
    inst.path = rootPath;

    // GameData 缺少必需的 Squad 目录 → 判定安装损坏，中止添加
    bool corrupted = false;
    ckan::GameInstance::detectInstallKindTags(rootPath, &corrupted);
    if (corrupted) {
        QMessageBox::warning(this, tr("错误"),
            tr("所选 GameData 目录缺少运行必需的 Squad 文件夹，游戏安装可能已损坏，无法添加实例。"));
        return;
    }

    // 使用建议名称：KSP + 版本号 + 安装类型标签（如 "KSP 1.12.5 RSS RO"），重名自动追加序号
    QSet<QString> usedNames;
    for (const KSPInstance &existing : ConfigManager::instance().instances())
        usedNames.insert(existing.name);
    inst.name = makeUniqueInstanceName(ckan::GameInstance::suggestedInstanceName(rootPath), usedNames);

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
    // 已存在实例名集合，用于建议名称去重
    QSet<QString> usedNames;
    const QList<KSPInstance> instances = ConfigManager::instance().instances();
    for (const KSPInstance &inst : instances) {
        existingPaths.insert(QDir::cleanPath(inst.path));
        usedNames.insert(inst.name);
    }

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

        // 名称 = KSP + 检测到的版本号 + 安装类型标签（如 "KSP 1.12.5 Clean Stock"），重名自动追加序号
        inst.name = makeUniqueInstanceName(ckan::GameInstance::suggestedInstanceName(cleanDir), usedNames);

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

QString MainWindow::makeUniqueInstanceName(const QString &base, QSet<QString> &usedNames) const
{
    if (!usedNames.contains(base)) {
        usedNames.insert(base);
        return base;
    }
    int n = 2;
    QString candidate;
    do {
        candidate = QStringLiteral("%1 (%2)").arg(base).arg(n++);
    } while (usedNames.contains(candidate));
    usedNames.insert(candidate);
    return candidate;
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
    // 实例管理详情页 / 存档管理 / 存档详情页均自带二级菜单，隐藏全局主侧边栏
    // 统一让实例二级菜单占位，返回顶层页（首页/实例列表/设置）时再显示。
    const bool instanceSubPage =
        (page == m_instanceDetailPage) ||
        (page == m_savesListPage) ||
        (page == m_saveDetailPage);
    if (m_sidebar)
        m_sidebar->setVisible(!instanceSubPage);
}

void MainWindow::onLaunchClicked()
{
    if (m_gameRunning) {
        QMessageBox::StandardButton ret = QMessageBox::warning(
            this, tr("停止游戏"),
            tr("您确定要终止游戏进程吗，这可能会丢失数据。"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ret == QMessageBox::Yes) {
            m_stoppingGame = true;
            InstanceManager::instance().stopGame();
        }
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

    bool launched = InstanceManager::instance().launchGame(
        inst.exePath, inst.launchArgs, inst.launchMemoryMB, inst.launchHighPriority);
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
    m_stoppingGame = false;
    m_launchButton->setProperty("stopActive", true);
    m_launchButton->style()->unpolish(m_launchButton);
    m_launchButton->style()->polish(m_launchButton);
    m_launchButton->setIcon(IconUtils::tintedIcon(":/icons/stop.svg", "#ffffff"));
    m_launchButton->setText(tr(" 停止"));
    m_launchButton->setEnabled(true);

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

void MainWindow::resetLaunchButton()
{
    m_gameRunning = false;
    m_stoppingGame = false;
    m_launchButton->setProperty("stopActive", false);
    m_launchButton->style()->unpolish(m_launchButton);
    m_launchButton->style()->polish(m_launchButton);
    m_launchButton->setIcon(IconUtils::tintedIcon(":/icons/rocket.svg", IconUtils::iconColorForTheme(m_currentTheme)));
    m_launchButton->setText(tr(" 启动游戏"));
    m_launchButton->setEnabled(true);
}

void MainWindow::onGameFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(exitCode);
    Q_UNUSED(status);
    bool wasStoppedByButton = m_stoppingGame;
    resetLaunchButton();
    // 按钮触发的终止保持窗口状态；游戏自行退出且启动器被最小化时恢复正常窗口
    if (!wasStoppedByButton && ConfigManager::instance().launchBehavior() == ConfigManager::Minimize) {
        showNormal();
        activateWindow();
    }
}

void MainWindow::onGameError(QProcess::ProcessError error)
{
    Q_UNUSED(error);
    bool wasStoppedByButton = m_stoppingGame;
    resetLaunchButton();
    // 按钮主动终止会触发 errorOccurred(Crashed)，此时不应误报"进程发生错误"
    if (wasStoppedByButton)
        return;
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
        m_launchButton->setEnabled(true);
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

void MainWindow::onSavesNavToDetail(int detailIndex)
{
    if (!toInstanceDetailPage())
        return;
    m_instanceDetailPage->showSection(detailIndex);
}

void MainWindow::onSavesModpackAction(int actionKind)
{
    if (!toInstanceDetailPage())
        return;
    m_instanceDetailPage->showSection(2); // 动作发生在地模组管理 tab
    if (actionKind == 0)
        m_instanceDetailPage->triggerExportModpack();
    else if (actionKind == 1)
        m_instanceDetailPage->triggerImportModpack();
    else if (actionKind == 2)
        m_instanceDetailPage->triggerBrowse();
}

// 切到实例管理详情页并隐藏启动条（当前实例为空时返回 false）
bool MainWindow::toInstanceDetailPage()
{
    KSPInstance cur = ConfigManager::instance().currentInstance();
    if (cur.id.isEmpty())
        return false;
    m_instanceDetailPage->loadCurrentInstance();
    setNavButtonChecked(m_instanceManageBtn);
    showPage(m_instanceDetailPage);
    m_launchBar->hide();
    return true;
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

void MainWindow::onSaveSelected(const QString &savePath, const QString &instanceName)
{
    m_saveDetailPage->setSavePath(savePath, instanceName);
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

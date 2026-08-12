#include "instancemanager.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QMap>

namespace {
QMap<QString, QString> createKeyTranslationMap() {
    QMap<QString, QString> m;
    // 显示设置
    m["SCREEN_RESOLUTION_WIDTH"] = "屏幕宽度";
    m["SCREEN_RESOLUTION_HEIGHT"] = "屏幕高度";
    m["FULLSCREEN"] = "全屏模式";
    m["BORDERLESS"] = "无边框窗口";
    m["ANTI_ALIASING"] = "抗锯齿";
    m["QUALITY_PRESET"] = "画质预设";
    m["TEXTURE_QUALITY"] = "纹理质量";
    m["SHADOWS_QUALITY"] = "阴影质量";
    m["REFLECTIONS_QUALITY"] = "反射质量";
    m["RENDER_TERRAIN_DETAIL"] = "地形细节";
    m["PARTICLES_QUALITY"] = "粒子效果质量";
    m["V_SYNC"] = "垂直同步";
    m["FRAME_LIMITER"] = "帧率限制";
    m["SCREENSHOT_RESOLUTION_SCALE"] = "截图分辨率缩放";
    // 音频
    m["VOLUME_VOICE"] = "语音音量";
    m["VOLUME_SFX"] = "音效音量";
    m["VOLUME_UI"] = "界面音量";
    m["VOLUME_MUSIC"] = "音乐音量";
    m["VOLUME_AMBIENCE"] = "环境音量";
    m["VOICE_MUTE"] = "静音语音";
    m["SFX_MUTE"] = "静音音效";
    m["UI_MUTE"] = "静音界面";
    m["MUSIC_MUTE"] = "静音音乐";
    m["AMBIENCE_MUTE"] = "静音环境";
    m["AUDIO_DEVICE"] = "音频设备";
    // 游戏玩法
    m["DIFFICULTY"] = "游戏难度";
    m["DIFFICULTY_PRESET"] = "难度预设";
    m["QUICKSAVE_BEHAVIOR"] = "快速存档模式";
    m["QUICKSAVE_LIMIT"] = "快速存档数量限制";
    m["AUTO_SAVE_INTERVAL"] = "自动存档间隔";
    m["MISSING_PARTS_IN_VAB"] = "VAB中允许缺少部件";
    m["MISSING_PARTS_IN_SPH"] = "SPH中允许缺少部件";
    m["MISSING_PARTS_IN_FLIGHT"] = "飞行时允许缺少部件";
    m["ALLOW_STAGING_LOCK"] = "允许分级锁定";
    m["SHOW_PARTS_IN_PARTS_SELECTOR"] = "显示部件在选择器中";
    m["PART_LIST_FONT_SIZE"] = "部件列表字体大小";
    m["CREW_HAPPINESS"] = "船员快乐度";
    m["KERBAL_LEVELS"] = "坎巴拉人等级系统";
    // 控制
    m["INVERT_MOUSE"] = "反转鼠标Y轴";
    m["MOUSE_SENSITIVITY"] = "鼠标灵敏度";
    m["VESSEL_VIEW_KEYBIND"] = "飞船视角快捷键";
    m["MAP_VIEW_KEYBIND"] = "地图视角快捷键";
    m["PAUSE_KEYBIND"] = "暂停快捷键";
    m["QUICKSAVE_KEYBIND"] = "快速存档快捷键";
    m["QUICKLOAD_KEYBIND"] = "快速读档快捷键";
    m["STAGE_KEYBIND"] = "分级快捷键";
    m["THROTTLE_CUTOFF_KEYBIND"] = "油门关闭快捷键";
    m["THROTTLE_FULL_KEYBIND"] = "油门全开快捷键";
    // 其他
    m["LANGUAGE"] = "游戏语言";
    m["CONIC_PATCH_MODE"] = "轨道显示模式";
    m["CONIC_PATCH_LIMIT"] = "轨道显示数量";
    m["SHOW_TIMER_IN_FPS_LIMITER"] = "帧率限制显示计时器";
    m["MOMENT_WHEEL_THRUST"] = "反作用轮推力倍数";
    m["MOMENT_WHEEL_TORQUE"] = "反作用轮扭矩倍数";
    m["WHEEL_TRACTION"] = "车轮摩擦力";
    m["WHEEL_DAMPING"] = "车轮阻尼";
    m["WHEEL_SUSPENSION"] = "车轮悬挂";
    m["AERO_FORCES"] = "空气动力学力";
    m["AERO_DISPLAY_GIZMOS"] = "显示空气动力学Gizmos";
    m["THERMAL_DEBUGGING"] = "热力学调试显示";
    m["COMMNET_SIGNAL"] = "通讯网络信号";
    m["COMMNET_PLURAL"] = "通讯网络多天线";
    m["ABLATION"] = "烧蚀系统";
    m["REENTRY_HEATING"] = "再入加热";
    m["HEAT_GAINS"] = "热量增益";
    return m;
}

QString translateKey(const QString& key) {
    static QMap<QString, QString> tr = createKeyTranslationMap();
    return tr.value(key, key);
}
}

InstanceManager& InstanceManager::instance()
{
    static InstanceManager inst;
    return inst;
}

InstanceManager::InstanceManager(QObject *parent)
    : QObject(parent), m_gameProcess(nullptr)
{
}

InstanceManager::~InstanceManager()
{
    if (m_gameProcess && m_gameProcess->state() != QProcess::NotRunning) {
        m_gameProcess->kill();
        m_gameProcess->waitForFinished(3000);
    }
    delete m_gameProcess;
}

QList<GameSetting> InstanceManager::loadGameSettings(const QString &gamePath) const
{
    QList<GameSetting> settings;
    QString settingsPath = QDir(gamePath).filePath("settings.cfg");
    QFile file(settingsPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return settings;
    }

    QTextStream in(&file);
    int braceDepth = 0;
    bool inBlock = false;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith("//")) {
            continue;
        }

        if (line.contains('{')) {
            braceDepth++;
            if (braceDepth == 1 && !inBlock) {
                inBlock = true;
            }
            continue;
        }
        if (line.contains('}')) {
            braceDepth--;
            if (braceDepth == 0) {
                inBlock = false;
            }
            continue;
        }

        if (braceDepth == 0 && line.contains('=')) {
            int eqPos = line.indexOf('=');
            QString key = line.left(eqPos).trimmed();
            // Skip version entry
            if (key == "SETTINGS_FILE_VERSION") {
                continue;
            }
            GameSetting s;
            s.key = key;
            s.value = line.mid(eqPos + 1).trimmed();
            s.displayName = translateKey(key);
            settings.append(s);
        }
    }

    file.close();
    return settings;
}

bool InstanceManager::saveGameSettings(const QString &gamePath, const QList<GameSetting> &settings) const
{
    QString settingsPath = QDir(gamePath).filePath("settings.cfg");
    QFile file(settingsPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    // Read original lines to preserve comments and structure
    QStringList originalLines;
    QTextStream in(&file);
    while (!in.atEnd()) {
        originalLines.append(in.readLine());
    }
    file.close();

    // Build key-value map from new settings
    QMap<QString, QString> valueMap;
    for (const GameSetting& s : settings) {
        valueMap[s.key] = s.value;
    }

    // Rewrite lines updating values
    int braceDepth = 0;
    bool inBlock = false;
    for (int i = 0; i < originalLines.size(); ++i) {
        QString line = originalLines[i];
        QString trimmed = line.trimmed();

        if (trimmed.contains('{')) {
            braceDepth++;
            if (braceDepth == 1 && !inBlock) {
                inBlock = true;
            }
            continue;
        }
        if (trimmed.contains('}')) {
            braceDepth--;
            if (braceDepth == 0) {
                inBlock = false;
            }
            continue;
        }

        if (braceDepth == 0 && trimmed.contains('=') && !trimmed.startsWith("//")) {
            int eqPos = trimmed.indexOf('=');
            QString key = trimmed.left(eqPos).trimmed();
            if (valueMap.contains(key)) {
                int originalEqPos = line.indexOf('=');
                QString prefix = line.left(originalEqPos + 1);
                originalLines[i] = prefix + " " + valueMap[key];
            }
        }
    }

    // Write back
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }
    QTextStream out(&file);
    for (const QString& l : originalLines) {
        out << l << "\n";
    }
    file.close();
    return true;
}

QList<DLCDetection> InstanceManager::detectDLCs(const QString &gamePath) const
{
    QList<DLCDetection> dlcs;

    DLCDetection mh;
    mh.id = "MakingHistory";
    mh.displayName = "Making History";
    mh.installed = QDir(QDir(gamePath).filePath("GameData/SquadExpansion/MakingHistory")).exists();
    dlcs.append(mh);

    DLCDetection bg;
    bg.id = "Serenity";
    bg.displayName = "Breaking Ground";
    bg.installed = QDir(QDir(gamePath).filePath("GameData/SquadExpansion/Serenity")).exists();
    dlcs.append(bg);

    return dlcs;
}

QStringList InstanceManager::listMods(const QString &gamePath) const
{
    QStringList mods;
    QString gameDataPath = QDir(gamePath).filePath("GameData");
    QDir gameData(gameDataPath);
    if (!gameData.exists()) {
        return mods;
    }

    QStringList entries = gameData.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    QStringList excluded = {"Squad", "SquadExpansion"};
    for (const QString& entry : entries) {
        if (!excluded.contains(entry, Qt::CaseInsensitive)) {
            mods.append(entry);
        }
    }

    return mods;
}

bool InstanceManager::launchGame(const QString &exePath)
{
    if (m_gameProcess && m_gameProcess->state() != QProcess::NotRunning) {
        return false;
    }

    if (!m_gameProcess) {
        m_gameProcess = new QProcess(this);
        connect(m_gameProcess, &QProcess::started, this, &InstanceManager::gameStarted);
        connect(m_gameProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &InstanceManager::gameFinished);
        connect(m_gameProcess, &QProcess::errorOccurred, this, &InstanceManager::gameError);
    }

    QFileInfo exeInfo(exePath);
    m_gameProcess->setWorkingDirectory(exeInfo.absolutePath());
    m_gameProcess->start(exePath, QStringList());
    return true;
}

QString InstanceManager::detectGameRoot(const QString &exePath) const
{
    QFileInfo fi(exePath);
    return fi.absolutePath();
}

bool InstanceManager::isValidKSPPath(const QString &path) const
{
    QDir dir(path);
    return dir.exists("settings.cfg") && dir.exists("GameData");
}

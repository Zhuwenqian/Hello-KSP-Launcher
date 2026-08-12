#ifndef INSTANCEMANAGER_H
#define INSTANCEMANAGER_H

#include <QObject>
#include <QMap>
#include <QStringList>
#include <QProcess>

struct GameSetting {
    QString key;        // 原始键名 (如 SCREEN_RESOLUTION_WIDTH)
    QString value;      // 值
    QString displayName;// 中文显示名
};

struct DLCDetection {
    QString id;
    QString displayName;
    bool installed;
};

class InstanceManager : public QObject
{
    Q_OBJECT
public:
    static InstanceManager& instance();

    QList<GameSetting> loadGameSettings(const QString& gamePath) const;
    bool saveGameSettings(const QString& gamePath, const QList<GameSetting>& settings) const;
    QList<DLCDetection> detectDLCs(const QString& gamePath) const;
    QStringList listMods(const QString& gamePath) const;
    bool launchGame(const QString& exePath);
    QString detectGameRoot(const QString& exePath) const;
    bool isValidKSPPath(const QString& path) const;

signals:
    void gameStarted();
    void gameFinished(int exitCode, QProcess::ExitStatus status);
    void gameError(QProcess::ProcessError error);

private:
    explicit InstanceManager(QObject *parent = nullptr);
    ~InstanceManager();
    InstanceManager(const InstanceManager&) = delete;
    InstanceManager& operator=(const InstanceManager&) = delete;

    QProcess* m_gameProcess;
};

#endif // INSTANCEMANAGER_H

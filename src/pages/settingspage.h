#ifndef SETTINGSPAGE_H
#define SETTINGSPAGE_H

#include <QWidget>
#include <QScrollArea>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include "../configmanager.h"
#include "../widgets/toggleswitch.h"

class SettingsPage : public QWidget
{
    Q_OBJECT
public:
    explicit SettingsPage(QWidget *parent = nullptr);

signals:
    void themeChanged(const QString& theme);

private slots:
    void onLanguageChanged(int index);
    void onLaunchBehaviorChanged(int index);
    void onThemeChanged(int index);
    void loadSettings();
    void onChooseBackgroundClicked();
    void onResetBackgroundClicked();
    void refreshBackgroundPreview();
    void onIndexIntervalChanged(int index);
    void onIndexSourceChanged(int index);
    void onModuleSourceChanged(int index);
    void onChooseCacheDirClicked();
    void onClearCacheClicked();
    void onConcurrencyChanged(int index);
    void onRateLimitEdited();
    void onAddPresetRepo(const ckan::Repository &preset);
    void onAddCustomRepo();
    void onRemoveRepo();
    void onMoveRepoUp();
    void onMoveRepoDown();
    void onRefreshRepos();

private:
    void setupUI();
    void loadRepoList();
    void applyRepos(const QVector<ckan::Repository> &repos, bool refresh = true);

    QComboBox* m_languageCombo;
    QComboBox* m_behaviorCombo;
    QComboBox* m_themeCombo;

    // 背景图片相关
    QLabel* m_backgroundPathLabel;
    QLabel* m_backgroundPreviewLabel;
    QPushButton* m_chooseBackgroundBtn;
    QPushButton* m_resetBackgroundBtn;

    // 模组管理相关
    QComboBox* m_indexIntervalCombo;
    QComboBox* m_indexSourceCombo;
    QComboBox* m_moduleSourceCombo;
    QComboBox* m_concurrencyCombo;
    QLineEdit* m_rateLimitEdit; // 下载限速（MB/秒，每链接）
    QLabel* m_cacheDirLabel;
    ToggleSwitch* m_installSuggestsToggle;
    ToggleSwitch* m_diskSpaceCheckToggle;

    // 仓库列表相关
    QListWidget* m_repoList;

    QScrollArea* m_scrollArea;
};

#endif // SETTINGSPAGE_H

#ifndef SETTINGSPAGE_H
#define SETTINGSPAGE_H

#include <QWidget>
#include <QScrollArea>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include "../configmanager.h"

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

private:
    void setupUI();

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
    QLabel* m_cacheDirLabel;

    QScrollArea* m_scrollArea;
};

#endif // SETTINGSPAGE_H

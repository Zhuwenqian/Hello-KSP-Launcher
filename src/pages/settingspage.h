#ifndef SETTINGSPAGE_H
#define SETTINGSPAGE_H

#include <QWidget>
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
};

#endif // SETTINGSPAGE_H

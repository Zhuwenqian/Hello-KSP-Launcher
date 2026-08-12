#ifndef SETTINGSPAGE_H
#define SETTINGSPAGE_H

#include <QWidget>
#include <QComboBox>
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

private:
    void setupUI();

    QComboBox* m_languageCombo;
    QComboBox* m_behaviorCombo;
    QComboBox* m_themeCombo;
};

#endif // SETTINGSPAGE_H

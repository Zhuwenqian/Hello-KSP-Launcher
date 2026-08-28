#ifndef SAVESLISTPAGE_H
#define SAVESLISTPAGE_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include "../configmanager.h"

class SavesListPage : public QWidget
{
    Q_OBJECT
public:
    explicit SavesListPage(QWidget *parent = nullptr);

    void setInstanceId(const QString& id);
    void loadSaves();
    void refreshIcons(const QString& color);

signals:
    void backClicked();
    void saveSelected(const QString& saveFolderPath, const QString& instanceName);

private slots:
    void onBackClicked();
    void onSaveItemDoubleClicked(QListWidgetItem* item);

private:
    void setupUI();

    QString m_instanceId;
    KSPInstance m_instance;

    QPushButton* m_backButton;
    QLabel* m_titleLabel;
    QListWidget* m_savesList;
};

#endif // SAVESLISTPAGE_H

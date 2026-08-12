#ifndef INSTANCELISTPAGE_H
#define INSTANCELISTPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QPushButton>
#include <QMenu>
#include "../configmanager.h"
#include "../widgets/instanceitemwidget.h"

class InstanceListPage : public QWidget
{
    Q_OBJECT
public:
    explicit InstanceListPage(QWidget *parent = nullptr);

signals:
    void addInstanceRequested();
    void instanceEntered(const QString& id);
    void instanceSelected(const QString& id);
    void currentInstanceChanged();

public slots:
    void refresh();

private slots:
    void onAddInstanceClicked();
    void onInstanceClicked(const QString& id);
    void onInstanceCheckboxToggled(const QString& id, bool checked);
    void onInstanceMenuRequested(const QString& id, QPoint pos);
    void onRenameInstance();
    void onDeleteInstance();
    void doRefresh();

private:
    QScrollArea* m_scrollArea;
    QWidget* m_listContainer;
    QVBoxLayout* m_listLayout;
    QPushButton* m_addButton;
    QList<InstanceItemWidget*> m_items;
    QMenu* m_contextMenu;
    QString m_menuInstanceId;
    QString m_selectedInstanceId;
    bool m_refreshPending;
};

#endif // INSTANCELISTPAGE_H

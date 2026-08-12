#ifndef INSTANCEITEMWIDGET_H
#define INSTANCEITEMWIDGET_H

#include <QWidget>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include "../configmanager.h"

class InstanceItemWidget : public QWidget
{
    Q_OBJECT
public:
    explicit InstanceItemWidget(const KSPInstance& instance, bool selected = false, QWidget *parent = nullptr);

    void setSelected(bool selected);
    bool isSelected() const;
    KSPInstance instance() const;

signals:
    void clicked(const QString& instanceId);
    void checkboxToggled(const QString& instanceId, bool checked);
    void menuRequested(const QString& instanceId, QPoint pos);

protected:
    void mousePressEvent(QMouseEvent *event) override;

private slots:
    void onCheckboxToggled(bool checked);
    void onMenuButtonClicked();

private:
    KSPInstance m_instance;
    bool m_selected;
    QCheckBox* m_checkbox;
    QLabel* m_nameLabel;
    QLabel* m_pathLabel;
    QPushButton* m_menuButton;
};

#endif // INSTANCEITEMWIDGET_H

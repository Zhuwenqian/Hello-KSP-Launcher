#ifndef HOMEPAGE_H
#define HOMEPAGE_H

#include <QWidget>
#include <QLabel>
#include <QFrame>
#include "../configmanager.h"

class HomePage : public QWidget
{
    Q_OBJECT
public:
    explicit HomePage(QWidget *parent = nullptr);

    void refreshCurrentInstance();

private:
    void setupUI();

    QLabel* m_instanceNameLabel;
    QLabel* m_instancePathLabel;
    QLabel* m_statusLabel;
    QFrame* m_infoFrame;
};

#endif // HOMEPAGE_H

#include "instanceitemwidget.h"
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QStyle>

InstanceItemWidget::InstanceItemWidget(const KSPInstance &instance, bool selected, QWidget *parent)
    : QWidget(parent), m_instance(instance), m_selected(selected)
{
    setObjectName("instanceItem");
    setMinimumHeight(60);
    setProperty("selected", m_selected);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(15, 10, 15, 10);
    layout->setSpacing(10);

    m_checkbox = new QCheckBox(this);
    m_checkbox->setObjectName("instanceCheckbox");
    m_checkbox->setChecked(m_selected);
    connect(m_checkbox, &QCheckBox::toggled, this, &InstanceItemWidget::onCheckboxToggled);

    QWidget* textWidget = new QWidget(this);
    textWidget->setAttribute(Qt::WA_TransparentForMouseEvents);
    QVBoxLayout* textLayout = new QVBoxLayout(textWidget);
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(2);

    m_nameLabel = new QLabel(m_instance.name, textWidget);
    m_nameLabel->setObjectName("instanceName");

    m_pathLabel = new QLabel(m_instance.path, textWidget);
    m_pathLabel->setObjectName("instancePath");

    textLayout->addWidget(m_nameLabel);
    textLayout->addWidget(m_pathLabel);

    m_menuButton = new QPushButton("...", this);
    m_menuButton->setObjectName("instanceMenuButton");
    m_menuButton->setFixedSize(30, 30);
    connect(m_menuButton, &QPushButton::clicked, this, &InstanceItemWidget::onMenuButtonClicked);

    layout->addWidget(m_checkbox);
    layout->addWidget(textWidget, 1);
    layout->addWidget(m_menuButton);
}

void InstanceItemWidget::setSelected(bool selected)
{
    m_selected = selected;
    m_checkbox->setChecked(selected);
    setProperty("selected", selected);
    style()->unpolish(this);
    style()->polish(this);
}

bool InstanceItemWidget::isSelected() const
{
    return m_selected;
}

KSPInstance InstanceItemWidget::instance() const
{
    return m_instance;
}

void InstanceItemWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_checkbox->geometry().contains(event->pos())) {
            m_checkbox->setChecked(!m_checkbox->isChecked());
        } else if (m_menuButton->geometry().contains(event->pos())) {
            // menu button handles its own click
        } else {
            emit clicked(m_instance.id);
        }
    }
    QWidget::mousePressEvent(event);
}

void InstanceItemWidget::onCheckboxToggled(bool checked)
{
    m_selected = checked;
    setProperty("selected", checked);
    style()->unpolish(this);
    style()->polish(this);
    emit checkboxToggled(m_instance.id, checked);
}

void InstanceItemWidget::onMenuButtonClicked()
{
    QPoint pos = m_menuButton->mapToGlobal(m_menuButton->rect().bottomRight());
    emit menuRequested(m_instance.id, pos);
}

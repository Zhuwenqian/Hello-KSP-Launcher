#include "instanceitemwidget.h"
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QStyle>
#include <QImage>
#include <QPixmap>
#include "../instanceiconmanager.h"

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

    // 实例图标：位于选择框右侧、实例名左侧，固定 40x40。
    m_iconLabel = new QLabel(this);
    m_iconLabel->setObjectName("instanceIcon");
    m_iconLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_iconLabel->setFixedSize(40, 40);
    m_iconLabel->setAlignment(Qt::AlignCenter);

    // 请求图标，并在异步提取完成后刷新本项。
    connect(&InstanceIconManager::instance(), &InstanceIconManager::iconReady,
            this, [this](const QString& id, const QImage& icon) {
                if (id == m_instance.id)
                    applyIcon(icon);
            });
    InstanceIconManager::instance().requestIcon(m_instance.id, m_instance.name, m_instance.exePath);

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
    layout->addWidget(m_iconLabel);
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

void InstanceItemWidget::applyIcon(const QImage& icon)
{
    if (icon.isNull()) {
        // 无可用图标时留空
        m_iconLabel->clear();
        return;
    }
    const qreal dpr = devicePixelRatioF();
    const int px = qRound(40 * dpr);
    QPixmap pm = QPixmap::fromImage(icon);
    QPixmap scaled = pm.scaled(px, px, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    scaled.setDevicePixelRatio(dpr);
    m_iconLabel->setPixmap(scaled);
}

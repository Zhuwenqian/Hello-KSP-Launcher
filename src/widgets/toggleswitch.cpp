#include "toggleswitch.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>

ToggleSwitch::ToggleSwitch(QWidget* parent)
    : QWidget(parent)
    , m_checked(false)
    , m_knobPos(0.0f)
    , m_animation(new QPropertyAnimation(this, "knobPosition", this))
{
    // Colors matching the design image
    trackOnColor  = QColor(55, 60, 90);    // dark blue-gray track (ON)
    trackOffColor = QColor(50, 50, 55);    // dark gray track (OFF)
    knobOnColor   = QColor(195, 195, 245); // light lavender knob (ON)
    knobOffColor  = QColor(150, 150, 155); // gray knob (OFF)

    setFixedSize(48, 26);
    setCursor(Qt::PointingHandCursor);

    m_animation->setDuration(200);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
}

bool ToggleSwitch::isChecked() const
{
    return m_checked;
}

void ToggleSwitch::setChecked(bool checked)
{
    if (m_checked == checked) return;
    m_checked = checked;
    animateTo(checked);
    emit toggled(checked);
}

float ToggleSwitch::knobPosition() const
{
    return m_knobPos;
}

void ToggleSwitch::setKnobPosition(float pos)
{
    m_knobPos = pos;
    update();
}

void ToggleSwitch::animateTo(bool checked)
{
    m_animation->stop();
    m_animation->setStartValue(m_knobPos);
    m_animation->setEndValue(checked ? 1.0f : 0.0f);
    m_animation->start();
}

void ToggleSwitch::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const int w = width();
    const int h = height();
    const int trackH = h;
    const int trackW = w;
    const int knobD = h - 4; // knob diameter with 2px padding
    const int knobR = knobD / 2;

    // Track: rounded rectangle
    const float trackRadius = trackH / 2.0f;
    QPainterPath trackPath;
    trackPath.addRoundedRect(0, 0, trackW, trackH, trackRadius, trackRadius);

    QColor trackColor = m_checked ? trackOnColor : trackOffColor;
    painter.setPen(Qt::NoPen);
    painter.setBrush(trackColor);
    painter.drawPath(trackPath);

    // Knob position
    const float padding = 2.0f;
    const float minX = padding;
    const float maxX = trackW - knobD - padding;
    const float knobX = minX + m_knobPos * (maxX - minX);
    const float knobY = (h - knobD) / 2.0f;

    QColor knobColor = m_checked ? knobOnColor : knobOffColor;
    painter.setBrush(knobColor);
    painter.drawEllipse(QPointF(knobX + knobR, knobY + knobR), knobR, knobR);
}

void ToggleSwitch::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        setChecked(!m_checked);
        event->accept();
    } else {
        QWidget::mousePressEvent(event);
    }
}

void ToggleSwitch::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
}

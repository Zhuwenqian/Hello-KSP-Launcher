#ifndef TOGGLESWITCH_H
#define TOGGLESWITCH_H

#include <QWidget>
#include <QPropertyAnimation>

class ToggleSwitch : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(float knobPosition READ knobPosition WRITE setKnobPosition)

public:
    explicit ToggleSwitch(QWidget* parent = nullptr);

    bool isChecked() const;
    void setChecked(bool checked);

signals:
    void toggled(bool checked);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void animateTo(bool checked);

    float knobPosition() const;
    void setKnobPosition(float pos);

    bool m_checked;
    float m_knobPos; // 0.0 = left (off), 1.0 = right (on)
    QPropertyAnimation* m_animation;

    // Colors matching the design
    QColor trackOnColor;
    QColor trackOffColor;
    QColor knobOnColor;
    QColor knobOffColor;
};

#endif // TOGGLESWITCH_H

#pragma once

#include <QPainter>
#include <QSvgRenderer>
#include <QWidget>

class AvatarWidget final : public QWidget
{
public:
    explicit AvatarWidget(QWidget *parent = nullptr)
        : QWidget(parent), m_renderer(QStringLiteral(":/icons/avatar-placeholder.svg"))
    {
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        // 在控件的绘制设备上渲染矢量图，并保留屏幕 DPR。
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        m_renderer.render(&painter, QRectF(rect()));
    }

private:
    QSvgRenderer m_renderer;
};

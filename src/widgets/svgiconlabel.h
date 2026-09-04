#pragma once

#include <QLabel>
#include <QPainter>
#include <QSvgRenderer>

class SvgIconLabel final : public QLabel
{
    Q_OBJECT
    Q_PROPERTY(QString source READ source WRITE setSource)

public:
    explicit SvgIconLabel(QWidget *parent = nullptr) : QLabel(parent) {}
    QString source() const { return m_source; }
    void setSource(const QString &source)
    {
        m_source = source;
        m_renderer.load(source);
        update();
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        // 保留标签的背景样式，图标直接按屏幕像素密度绘制，不经过低分辨率位图。
        QLabel::paintEvent(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        m_renderer.render(&painter, QRectF((width() - 24) / 2.0, (height() - 24) / 2.0, 24, 24));
    }

private:
    QString m_source;
    QSvgRenderer m_renderer;
};

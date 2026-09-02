#ifndef TRENDCHARTWIDGET_H
#define TRENDCHARTWIDGET_H

#include "services/statisticsservice.h"

#include <QWidget>

class TrendChartWidget final : public QWidget
{
public:
    explicit TrendChartWidget(QWidget *parent = nullptr);

    void setPoints(QVector<DailyStatisticsPoint> points);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<DailyStatisticsPoint> m_points;
};

#endif // TRENDCHARTWIDGET_H

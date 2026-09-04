#ifndef SEATSHARECHARTWIDGET_H
#define SEATSHARECHARTWIDGET_H

#include "features/statistics/statisticsservice.h"

#include <QWidget>

class SeatShareChartWidget final : public QWidget
{
public:
    explicit SeatShareChartWidget(QWidget *parent = nullptr);

    void setShares(QVector<SeatSharePoint> shares);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<SeatSharePoint> m_shares;
};

#endif // SEATSHARECHARTWIDGET_H

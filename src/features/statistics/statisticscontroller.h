#ifndef STATISTICSCONTROLLER_H
#define STATISTICSCONTROLLER_H

#include "features/statistics/statisticsservice.h"

#include <QObject>

class DataStore;
class QLabel;

class StatisticsController final : public QObject
{
public:
    StatisticsController(DataStore *dataStore, QLabel *soldValue, QLabel *refundedValue,
                         QLabel *revenueValue, QLabel *rateValue, QObject *parent = nullptr);

private:
    void refresh();
    void updateMetrics(const StatisticsSummary &summary);

    DataStore *m_dataStore;
    QLabel *m_soldValue;
    QLabel *m_refundedValue;
    QLabel *m_revenueValue;
    QLabel *m_rateValue;
};

#endif // STATISTICSCONTROLLER_H

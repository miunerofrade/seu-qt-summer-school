#ifndef STATISTICSSERVICE_H
#define STATISTICSSERVICE_H

#include <QtGlobal>

class DataStore;

struct StatisticsSummary
{
    int soldCount = 0;
    int refundedCount = 0;
    qint64 netRevenueCents = 0;
    double averageRemainingRate = 0.0;
};

class StatisticsService final
{
public:
    explicit StatisticsService(const DataStore *dataStore);

    StatisticsSummary summarize() const;

private:
    const DataStore *m_dataStore;
};

#endif // STATISTICSSERVICE_H

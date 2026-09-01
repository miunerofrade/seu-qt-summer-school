#ifndef STATISTICSSERVICE_H
#define STATISTICSSERVICE_H

#include "domain/entities.h"

#include <QDate>
#include <QString>
#include <QVector>

class DataStore;

struct StatisticsFilter
{
    QDate from;
    QDate to;
    QString trainNumber;
    QString stationCode;
    QString seatType;
};

struct StatisticsRow
{
    QString trainNumber;
    QString route;
    int soldCount = 0;
    int refundedCount = 0;
    qint64 netRevenueCents = 0;
    double remainingRate = 0.0;
};

struct StatisticsSummary
{
    int soldCount = 0;
    int refundedCount = 0;
    qint64 netRevenueCents = 0;
    double averageRemainingRate = 0.0;
    QVector<StatisticsRow> rows;
};

class StatisticsService final
{
public:
    explicit StatisticsService(const DataStore *dataStore);

    StatisticsSummary summarize(const StatisticsFilter &filter) const;

private:
    const DataStore *m_dataStore;
};

#endif // STATISTICSSERVICE_H

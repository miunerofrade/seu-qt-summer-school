#ifndef QUERYSERVICE_H
#define QUERYSERVICE_H

#include "domain/entities.h"

#include <QDate>
#include <QTime>
#include <QVector>

class DataStore;

struct TrainQueryRequest
{
    QString departureStationCode;
    QString arrivalStationCode;
    QDate serviceDate;
};

struct TrainQueryRow
{
    QString trainNumber;
    QString departureStationName;
    QString arrivalStationName;
    QTime departureTime;
    QTime arrivalTime;
    int departureDayOffset = 0;
    int arrivalDayOffset = 0;
    int durationMinutes = 0;
    QString seatType;
    int remainingSeats = 0;
    qint64 priceCents = 0;
};

class QueryService final
{
public:
    explicit QueryService(const DataStore *dataStore);

    QVector<TrainQueryRow> query(const TrainQueryRequest &request) const;

private:
    const DataStore *m_dataStore;
};

#endif // QUERYSERVICE_H

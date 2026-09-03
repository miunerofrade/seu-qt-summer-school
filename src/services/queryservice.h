#ifndef QUERYSERVICE_H
#define QUERYSERVICE_H

#include "domain/entities.h"

#include <QDate>
#include <QDateTime>
#include <QTime>
#include <QVector>

class DataStore;

struct TrainQueryRequest
{
    QString departureStationCode;
    QString arrivalStationCode;
    QDate serviceDate;
};

struct TrainSeatOption
{
    QString seatType;
    int remainingSeats = 0;
    qint64 priceCents = 0;
    QString availabilityText;
};

struct TrainQueryRow
{
    QString trainNumber;
    QDate serviceDate;
    QString departureStationCode;
    QString arrivalStationCode;
    QString originStationName;
    QString terminalStationName;
    QString departureStationName;
    QString arrivalStationName;
    QTime departureTime;
    QTime arrivalTime;
    int departureDayOffset = 0;
    int arrivalDayOffset = 0;
    int durationMinutes = 0;
    QVector<TrainSeatOption> seats;
    bool bookable = true;
};

class QueryService final
{
public:
    explicit QueryService(const DataStore *dataStore);

    QVector<TrainQueryRow> query(const TrainQueryRequest &request,
                                 const QDateTime &notDepartedAfter = {}) const;
    QVector<TrainQueryRow> available(const QDateTime &notDepartedAfter) const;

private:
    const DataStore *m_dataStore;
};

#endif // QUERYSERVICE_H

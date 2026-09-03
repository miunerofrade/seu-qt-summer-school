#ifndef BOOKINGSERVICE_H
#define BOOKINGSERVICE_H

#include "common/result.h"

#include <QDate>
#include <QStringList>
#include <QTime>

class DataStore;

struct BookingRequest
{
    QString trainNumber;
    QDate serviceDate;
    QString departureStationCode;
    QString arrivalStationCode;
    QString seatType;
    QStringList passengerIds;
};

struct BookingReceipt
{
    QString orderId;
    QStringList ticketIds;
    qint64 totalAmountCents = 0;
};

struct DemoTrainSnapshot
{
    QString departureStationName;
    QString arrivalStationName;
    QTime departureTime;
    QTime arrivalTime;
    int departureDayOffset = 0;
    int arrivalDayOffset = 0;
    qint64 priceCents = 0;
    int remainingSeats = 0;
};

class BookingService final
{
public:
    explicit BookingService(DataStore *dataStore);

    OperationResult book(const BookingRequest &request, BookingReceipt *receipt = nullptr);
    OperationResult bookDemo(const BookingRequest &request,
                             const DemoTrainSnapshot &snapshot,
                             BookingReceipt *receipt = nullptr);

private:
    DataStore *m_dataStore;
};

#endif // BOOKINGSERVICE_H

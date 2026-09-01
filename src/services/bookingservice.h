#ifndef BOOKINGSERVICE_H
#define BOOKINGSERVICE_H

#include "common/result.h"

#include <QDate>
#include <QStringList>

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

class BookingService final
{
public:
    explicit BookingService(DataStore *dataStore);

    OperationResult book(const BookingRequest &request, BookingReceipt *receipt = nullptr);

private:
    DataStore *m_dataStore;
};

#endif // BOOKINGSERVICE_H

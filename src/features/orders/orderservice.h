#ifndef ORDERSERVICE_H
#define ORDERSERVICE_H

#include "common/result.h"
#include "domain/entities.h"

#include <QDateTime>
#include <QStringList>
#include <QVector>

class DataStore;

struct OrderSummary
{
    QString ownerUsername;
    QString orderId;
    QDateTime createdAt;
    QStringList passengerNames;
    QStringList trainNumbers;
    QDate serviceDate;
    QStringList seatTypes;
    qint64 totalAmountCents = 0;
    domain::TicketStatus status = domain::TicketStatus::Issued;
};

struct OrderTicketDetail
{
    QString ticketId;
    QString passengerName;
    QString trainNumber;
    QDate serviceDate;
    QString departureStationName;
    QString arrivalStationName;
    QDateTime departureAt;
    QDateTime arrivalAt;
    QString seatType;
    qint64 priceCents = 0;
    domain::TicketStatus status = domain::TicketStatus::Issued;
};

class OrderService final
{
public:
    explicit OrderService(DataStore *dataStore);

    QVector<OrderSummary> summaries() const;
    QVector<OrderTicketDetail> details(const QString &orderId) const;
    OperationResult refreshCompletedTickets(const QDateTime &now = QDateTime::currentDateTime());

    static QString statusText(domain::TicketStatus status);

private:
    DataStore *m_dataStore;
};

#endif // ORDERSERVICE_H

#ifndef DOMAIN_ENTITIES_H
#define DOMAIN_ENTITIES_H

#include <QDate>
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QTime>
#include <QVector>

namespace domain {

inline constexpr int CurrentSchemaVersion = 1;

enum class TicketStatus
{
    Issued,
    Refunded,
    Completed
};

struct Station
{
    QString code;
    QString name;
    QString city;
    bool enabled = true;
};

struct TrainStop
{
    QString stationCode;
    int sequence = 0;
    QTime arrivalTime;
    QTime departureTime;
    int dayOffset = 0;
};

struct SegmentInventory
{
    qint64 priceCents = 0;
    int totalSeats = 0;
    int remainingSeats = 0;
};

struct SeatInventory
{
    QString seatType;
    QVector<SegmentInventory> segments;
};

struct Train
{
    QString number;
    QDate serviceDate;
    bool enabled = true;
    bool saleOpen = true;
    QVector<TrainStop> stops;
    QVector<SeatInventory> seats;
};

struct Passenger
{
    QString id;
    QString name;
    QString documentType;
    QString documentNumber;
};

struct Order
{
    QString id;
    QDateTime createdAt;
    QStringList ticketIds;
    qint64 totalAmountCents = 0;
};

struct Ticket
{
    QString id;
    QString passengerId;
    QString trainNumber;
    QString fromStationCode;
    QString toStationCode;
    QString seatType;
    qint64 priceCents = 0;
    TicketStatus status = TicketStatus::Issued;
};

struct RefundRecord
{
    QString id;
    QString ticketId;
    QDateTime processedAt;
    int ratePercent = 0;
    qint64 feeCents = 0;
    qint64 refundAmountCents = 0;
};

struct AppData
{
    int schemaVersion = CurrentSchemaVersion;
    QVector<Station> stations;
    QVector<Train> trains;
    QVector<Passenger> passengers;
    QVector<Order> orders;
    QVector<Ticket> tickets;
    QVector<RefundRecord> refunds;
};

QString ticketStatusKey(TicketStatus status);
bool ticketStatusFromKey(const QString &key, TicketStatus *status);
AppData createDemoData(const QDate &serviceDate = QDate::currentDate());

} // namespace domain

#endif // DOMAIN_ENTITIES_H

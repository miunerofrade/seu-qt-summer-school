#ifndef DOMAIN_ENTITIES_H
#define DOMAIN_ENTITIES_H

#include <QDate>
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QTime>
#include <QVector>

namespace domain {

inline constexpr int CurrentSchemaVersion = 4;

struct User
{
    QString id;
    QString username;
    QString password;
    QString role;
};

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
    QVector<TrainStop> stops;
    QVector<SeatInventory> seats;
    // Empty id + invalid date denotes a daily recurring custom definition.
    // A valid date denotes one materialized service inventory; official
    // services additionally carry the opaque 12306 id.
    QString railwayTrainId;
    QDate railwayServiceDate;
};

struct Passenger
{
    QString id;
    QString name;
    QString documentType;
    QString documentNumber;
    QString ownerUserId = QStringLiteral("user-admin");
};

struct Order
{
    QString id;
    QDateTime createdAt;
    QStringList ticketIds;
    qint64 totalAmountCents = 0;
    QString ownerUserId = QStringLiteral("user-admin");
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
    QDate serviceDate;
    QString railwayTrainId;
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
    QStringList hiddenTrainNumbers;
    QVector<User> users = {{QStringLiteral("user-admin"), QStringLiteral("admin"),
                            QStringLiteral("admin"), QStringLiteral("admin")}};
};

QString ticketStatusKey(TicketStatus status);
bool ticketStatusFromKey(const QString &key, TicketStatus *status);
AppData createDemoData();

} // namespace domain

#endif // DOMAIN_ENTITIES_H

#ifndef DOMAIN_ENTITIES_H
#define DOMAIN_ENTITIES_H

#include <QDate>
#include <QDateTime>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QTime>
#include <QVector>

namespace domain {

inline constexpr int CurrentSchemaVersion = 7;
inline constexpr int DefaultSeatCount = 100;

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
};

struct SeatDetail
{
    QString seatId;
    QMap<QString, quint64> occupiedMasks;
};

struct SeatInventory
{
    QString seatType;
    QVector<SegmentInventory> segments;
    // 座位级、按乘车日期隔离的 occupiedMasks 是唯一库存事实。
    QVector<SeatDetail> details;
};

struct Train
{
    QString number;
    QVector<TrainStop> stops;
    QVector<SeatInventory> seats;
    // 空 ID 加无效日期表示每日重复的自定义定义。
    // 有效日期表示一个已实例化的服务库存；官方服务还会携带不透明的 12306 ID。
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
    QString seatId;
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
quint64 segmentMask(int fromIndex, int toIndex);
QString seatIdForIndex(const QString &seatType, int index, int firstCarriage = 1);
void rebuildSeatDetails(SeatInventory *inventory);
void renumberSeatDetails(SeatInventory *inventory);
void organizeTrainSeats(Train *train);
void organizeSeatAssignments(AppData *data);
quint64 occupiedMaskForDate(const SeatDetail &detail, const QDate &serviceDate);
void setOccupiedMaskForDate(SeatDetail *detail, const QDate &serviceDate, quint64 mask);
int availableSeatCount(const SeatInventory &inventory,
                       const QDate &serviceDate,
                       quint64 requestMask);
AppData createDemoData();

} // 命名空间 domain

#endif // DOMAIN_ENTITIES_H

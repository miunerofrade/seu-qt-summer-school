#include "domain/entities.h"

#include <QUuid>

namespace domain {

namespace {
QString newId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

TrainStop stop(const QString &stationCode,
               int sequence,
               const QTime &arrival,
               const QTime &departure,
               int dayOffset = 0)
{
    return {stationCode, sequence, arrival, departure, dayOffset};
}

SeatInventory seat(const QString &name,
                   std::initializer_list<SegmentInventory> segments)
{
    return {name, QVector<SegmentInventory>(segments)};
}
} // 命名空间

QString ticketStatusKey(TicketStatus status)
{
    switch (status) {
    case TicketStatus::Issued:
        return QStringLiteral("issued");
    case TicketStatus::Refunded:
        return QStringLiteral("refunded");
    case TicketStatus::Completed:
        return QStringLiteral("completed");
    }
    return QStringLiteral("issued");
}

bool ticketStatusFromKey(const QString &key, TicketStatus *status)
{
    if (!status)
        return false;
    if (key == QLatin1String("issued"))
        *status = TicketStatus::Issued;
    else if (key == QLatin1String("refunded"))
        *status = TicketStatus::Refunded;
    else if (key == QLatin1String("completed"))
        *status = TicketStatus::Completed;
    else
        return false;
    return true;
}

AppData createDemoData()
{
    AppData data;
    data.stations = {
        {QStringLiteral("NKH"), QStringLiteral("南京南"), QStringLiteral("南京"), true},
        {QStringLiteral("OHH"), QStringLiteral("苏州北"), QStringLiteral("苏州"), true},
        {QStringLiteral("AOH"), QStringLiteral("上海虹桥"), QStringLiteral("上海"), true},
        {QStringLiteral("HGH"), QStringLiteral("杭州东"), QStringLiteral("杭州"), true}
    };

    Train morning;
    morning.number = QStringLiteral("G101");
    morning.stops = {
        stop(QStringLiteral("NKH"), 0, {}, QTime(8, 0)),
        stop(QStringLiteral("OHH"), 1, QTime(8, 45), QTime(8, 47)),
        stop(QStringLiteral("AOH"), 2, QTime(9, 20), {})
    };
    morning.seats = {
        seat(QStringLiteral("二等座"), {{8500, 100, 40}, {6500, 100, 35}}),
        seat(QStringLiteral("一等座"), {{14000, 40, 12}, {11000, 40, 9}})
    };

    Train afternoon;
    afternoon.number = QStringLiteral("G205");
    afternoon.stops = {
        stop(QStringLiteral("NKH"), 0, {}, QTime(14, 10)),
        stop(QStringLiteral("AOH"), 1, QTime(15, 28), QTime(15, 32)),
        stop(QStringLiteral("HGH"), 2, QTime(16, 18), {})
    };
    afternoon.seats = {
        seat(QStringLiteral("二等座"), {{14900, 100, 58}, {7300, 100, 61}}),
        seat(QStringLiteral("一等座"), {{23800, 36, 15}, {11600, 36, 18}})
    };

    data.trains = {morning, afternoon};
    data.passengers = {
        {newId(), QStringLiteral("演示旅客1"), QStringLiteral("身份证"), QStringLiteral("320101199001010014")},
        {newId(), QStringLiteral("演示旅客2"), QStringLiteral("身份证"), QStringLiteral("32010119910101002X")},
        {newId(), QStringLiteral("演示旅客3"), QStringLiteral("身份证"), QStringLiteral("320101199201010035")},
        {newId(), QStringLiteral("演示旅客4"), QStringLiteral("身份证"), QStringLiteral("320101199301010040")},
        {newId(), QStringLiteral("演示旅客5"), QStringLiteral("身份证"), QStringLiteral("320101199401010056")},
        {newId(), QStringLiteral("演示旅客6"), QStringLiteral("身份证"), QStringLiteral("320101199501010061")},
        {newId(), QStringLiteral("演示旅客7"), QStringLiteral("身份证"), QStringLiteral("320101199601010077")},
        {newId(), QStringLiteral("演示旅客8"), QStringLiteral("身份证"), QStringLiteral("320101199701010082")},
        {newId(), QStringLiteral("演示旅客9"), QStringLiteral("身份证"), QStringLiteral("320101199801010098")},
        {newId(), QStringLiteral("演示旅客10"), QStringLiteral("身份证"), QStringLiteral("320101199901010108")}
    };
    return data;
}

} // 命名空间 domain

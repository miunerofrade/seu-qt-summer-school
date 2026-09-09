#include "domain/entities.h"

#include <QHash>
#include <QUuid>

#include <algorithm>
#include <limits>

namespace domain {

namespace {
struct DemoSegment
{
    qint64 priceCents;
    int totalSeats;
    int availableSeats;
};

QString seatPositions(const QString &seatType)
{
    if (seatType.contains(QStringLiteral("商务")))
        return QStringLiteral("ACF");
    if (seatType.contains(QStringLiteral("一等")))
        return QStringLiteral("ACDF");
    if (seatType.contains(QStringLiteral("二等")))
        return QStringLiteral("ABCDF");
    return QStringLiteral("ABCDE");
}

int seatTypeOrder(const QString &seatType)
{
    if (seatType.contains(QStringLiteral("商务"))) return 0;
    if (seatType.contains(QStringLiteral("特等"))) return 1;
    if (seatType.contains(QStringLiteral("一等"))) return 2;
    if (seatType.contains(QStringLiteral("二等"))) return 3;
    if (seatType.contains(QStringLiteral("高级软卧"))) return 4;
    if (seatType.contains(QStringLiteral("软卧"))) return 5;
    if (seatType.contains(QStringLiteral("动卧"))) return 6;
    if (seatType.contains(QStringLiteral("硬卧"))) return 7;
    if (seatType.contains(QStringLiteral("软座"))) return 8;
    if (seatType.contains(QStringLiteral("硬座"))) return 9;
    if (seatType.contains(QStringLiteral("无座"))) return 10;
    return 11;
}

int carriageCount(const SeatInventory &inventory)
{
    constexpr int RowsPerCarriage = 14;
    const int capacity = RowsPerCarriage * seatPositions(inventory.seatType).size();
    return capacity > 0 ? (inventory.details.size() + capacity - 1) / capacity : 0;
}

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
                   std::initializer_list<DemoSegment> segments)
{
    SeatInventory inventory;
    inventory.seatType = name;
    for (const DemoSegment &segment : segments)
        inventory.segments.append({segment.priceCents, segment.totalSeats});
    rebuildSeatDetails(&inventory);
    const QDate today = QDate::currentDate();
    for (int index = 0; index < inventory.details.size(); ++index) {
        quint64 mask = 0;
        int segmentIndex = 0;
        for (const DemoSegment &segment : segments) {
            if (index >= segment.availableSeats)
                mask |= quint64(1) << segmentIndex;
            ++segmentIndex;
        }
        setOccupiedMaskForDate(&inventory.details[index], today, mask);
    }
    return inventory;
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

quint64 segmentMask(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || toIndex <= fromIndex || toIndex > 64)
        return 0;
    quint64 mask = 0;
    for (int segment = fromIndex; segment < toIndex; ++segment)
        mask |= (quint64(1) << segment);
    return mask;
}

QString seatIdForIndex(const QString &seatType, int index, int firstCarriage)
{
    if (index < 0 || firstCarriage < 1)
        return {};
    const QString positions = seatPositions(seatType);
    constexpr int RowsPerCarriage = 14;
    const int globalRow = index / positions.size();
    const int carriage = firstCarriage + globalRow / RowsPerCarriage;
    const int row = globalRow % RowsPerCarriage + 1;
    const QChar position = positions.at(index % positions.size());
    return QStringLiteral("%1车%2%3")
        .arg(carriage, 2, 10, QLatin1Char('0'))
        .arg(row, 2, 10, QLatin1Char('0'))
        .arg(position);
}

void rebuildSeatDetails(SeatInventory *inventory)
{
    if (!inventory)
        return;
    int seatCount = 0;
    for (const SegmentInventory &segment : std::as_const(inventory->segments))
        seatCount = std::max(seatCount, segment.totalSeats);

    inventory->details.clear();
    inventory->details.reserve(seatCount);
    for (int index = 0; index < seatCount; ++index) {
        SeatDetail detail{seatIdForIndex(inventory->seatType, index), {}};
        inventory->details.append(std::move(detail));
    }
}

void renumberSeatDetails(SeatInventory *inventory)
{
    if (!inventory)
        return;
    for (int index = 0; index < inventory->details.size(); ++index)
        inventory->details[index].seatId = seatIdForIndex(inventory->seatType, index);
}

void organizeTrainSeats(Train *train)
{
    if (!train)
        return;
    std::stable_sort(train->seats.begin(), train->seats.end(), [](const SeatInventory &left,
                                                                  const SeatInventory &right) {
        const int leftOrder = seatTypeOrder(left.seatType);
        const int rightOrder = seatTypeOrder(right.seatType);
        return leftOrder < rightOrder
            || (leftOrder == rightOrder && left.seatType.localeAwareCompare(right.seatType) < 0);
    });

    int firstCarriage = 1;
    for (SeatInventory &inventory : train->seats) {
        for (int index = 0; index < inventory.details.size(); ++index)
            inventory.details[index].seatId = seatIdForIndex(inventory.seatType, index, firstCarriage);
        firstCarriage += carriageCount(inventory);
    }
}

void organizeSeatAssignments(AppData *data)
{
    if (!data)
        return;
    for (Train &train : data->trains) {
        QHash<QString, QVector<QString>> oldIdsBySeatType;
        for (const SeatInventory &seat : std::as_const(train.seats)) {
            QVector<QString> oldIds;
            oldIds.reserve(seat.details.size());
            for (const SeatDetail &detail : std::as_const(seat.details))
                oldIds.append(detail.seatId);
            oldIdsBySeatType.insert(seat.seatType.toCaseFolded(), oldIds);
        }

        organizeTrainSeats(&train);
        QHash<QString, QHash<QString, QString>> renamedIdsBySeatType;
        for (const SeatInventory &seat : std::as_const(train.seats)) {
            const QVector<QString> oldIds = oldIdsBySeatType.value(seat.seatType.toCaseFolded());
            QHash<QString, QString> renamedIds;
            for (int index = 0; index < seat.details.size() && index < oldIds.size(); ++index) {
                if (!oldIds.at(index).isEmpty())
                    renamedIds.insert(oldIds.at(index), seat.details.at(index).seatId);
            }
            renamedIdsBySeatType.insert(seat.seatType.toCaseFolded(), renamedIds);
        }

        for (Ticket &ticket : data->tickets) {
            bool trainMatches = false;
            if (!ticket.railwayTrainId.isEmpty()) {
                trainMatches = train.railwayTrainId == ticket.railwayTrainId
                    && train.railwayServiceDate == ticket.serviceDate;
            } else if (train.railwayTrainId.isEmpty()
                       && train.number.compare(ticket.trainNumber, Qt::CaseInsensitive) == 0) {
                trainMatches = !train.railwayServiceDate.isValid()
                    || train.railwayServiceDate == ticket.serviceDate;
            }
            if (!trainMatches)
                continue;
            const QHash<QString, QString> renamedIds =
                renamedIdsBySeatType.value(ticket.seatType.toCaseFolded());
            if (renamedIds.contains(ticket.seatId))
                ticket.seatId = renamedIds.value(ticket.seatId);
        }
    }
}

quint64 occupiedMaskForDate(const SeatDetail &detail, const QDate &serviceDate)
{
    if (!serviceDate.isValid())
        return 0;
    return detail.occupiedMasks.value(serviceDate.toString(Qt::ISODate), 0);
}

void setOccupiedMaskForDate(SeatDetail *detail, const QDate &serviceDate, quint64 mask)
{
    if (!detail || !serviceDate.isValid())
        return;
    const QString key = serviceDate.toString(Qt::ISODate);
    if (mask == 0)
        detail->occupiedMasks.remove(key);
    else
        detail->occupiedMasks.insert(key, mask);
}

int availableSeatCount(const SeatInventory &inventory,
                       const QDate &serviceDate,
                       quint64 requestMask)
{
    if (!serviceDate.isValid() || requestMask == 0)
        return 0;
    int capacity = inventory.details.isEmpty()
        ? std::numeric_limits<int>::max()
        : static_cast<int>(inventory.details.size());
    for (int segment = 0; segment < inventory.segments.size(); ++segment) {
        if ((requestMask & (quint64(1) << segment)) != 0)
            capacity = std::min(capacity, inventory.segments.at(segment).totalSeats);
    }
    if (capacity == std::numeric_limits<int>::max())
        return 0;
    if (inventory.details.isEmpty())
        return capacity;
    int available = 0;
    for (int index = 0; index < capacity; ++index) {
        if ((occupiedMaskForDate(inventory.details.at(index), serviceDate) & requestMask) == 0)
            ++available;
    }
    return available;
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
        seat(QStringLiteral("一等座"), {{14000, 100, 72}, {11000, 100, 69}}),
        seat(QStringLiteral("二等座"), {{8500, 100, 40}, {6500, 100, 35}})
    };
    organizeTrainSeats(&morning);

    Train afternoon;
    afternoon.number = QStringLiteral("G205");
    afternoon.stops = {
        stop(QStringLiteral("NKH"), 0, {}, QTime(14, 10)),
        stop(QStringLiteral("AOH"), 1, QTime(15, 28), QTime(15, 32)),
        stop(QStringLiteral("HGH"), 2, QTime(16, 18), {})
    };
    afternoon.seats = {
        seat(QStringLiteral("一等座"), {{23800, 100, 79}, {11600, 100, 82}}),
        seat(QStringLiteral("二等座"), {{14900, 100, 58}, {7300, 100, 61}})
    };
    organizeTrainSeats(&afternoon);

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

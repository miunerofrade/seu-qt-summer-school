#include "services/bookingservice.h"

#include "data/datastore.h"

#include <QUuid>

#include <algorithm>

namespace {
QString newId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

int findStop(const QVector<domain::TrainStop> &stops, const QString &stationCode)
{
    for (int i = 0; i < stops.size(); ++i) {
        if (stops.at(i).stationCode == stationCode)
            return i;
    }
    return -1;
}
}

BookingService::BookingService(DataStore *dataStore)
    : m_dataStore(dataStore)
{
}

OperationResult BookingService::book(const BookingRequest &request, BookingReceipt *receipt)
{
    if (!m_dataStore || m_dataStore->currentUserId().isEmpty())
        return OperationResult::failure(QStringLiteral("请先登录。"));
    if (!m_dataStore || request.trainNumber.trimmed().isEmpty() || !request.serviceDate.isValid()
        || request.departureStationCode.isEmpty() || request.arrivalStationCode.isEmpty()
        || request.departureStationCode == request.arrivalStationCode || request.seatType.trimmed().isEmpty()
        || request.passengerIds.isEmpty())
        return OperationResult::failure(QStringLiteral("购票信息不完整。"));

    QStringList passengerIds;
    for (const QString &id : request.passengerIds) {
        if (id.isEmpty() || passengerIds.contains(id))
            return OperationResult::failure(QStringLiteral("乘车人选择重复或无效。"));
        passengerIds.push_back(id);
    }

    domain::AppData candidate = m_dataStore->data();
    const auto stationEnabled = [&candidate](const QString &code) {
        const auto it = std::find_if(candidate.stations.cbegin(), candidate.stations.cend(),
                                     [&code](const domain::Station &station) {
                                         return station.code == code;
                                     });
        return it != candidate.stations.cend() && it->enabled;
    };
    if (!stationEnabled(request.departureStationCode) || !stationEnabled(request.arrivalStationCode))
        return OperationResult::failure(QStringLiteral("出发站或到达站已停用。"));

    int trainIndex = -1;
    for (int i = 0; i < candidate.trains.size(); ++i) {
        const domain::Train &train = candidate.trains.at(i);
        const bool identityMatches = request.railwayTrainId.isEmpty()
            ? train.railwayTrainId.isEmpty() && train.railwayServiceDate == request.serviceDate
            : train.railwayTrainId == request.railwayTrainId
                && train.railwayServiceDate == request.serviceDate;
        if (identityMatches
            && (!request.railwayTrainId.isEmpty() || train.number == request.trainNumber)) {
            trainIndex = i;
            break;
        }
    }
    if (trainIndex < 0 && request.railwayTrainId.isEmpty()) {
        const auto definition = std::find_if(candidate.trains.cbegin(), candidate.trains.cend(),
                                              [&request](const domain::Train &train) {
            return train.railwayTrainId.isEmpty() && !train.railwayServiceDate.isValid()
                && train.number == request.trainNumber;
        });
        if (definition != candidate.trains.cend()) {
            domain::Train occurrence = *definition;
            occurrence.railwayServiceDate = request.serviceDate;
            candidate.trains.append(std::move(occurrence));
            trainIndex = candidate.trains.size() - 1;
        }
    }
    if (trainIndex < 0)
        return OperationResult::failure(QStringLiteral("车次不存在或日期已变化，请重新查询。"));

    domain::Train &train = candidate.trains[trainIndex];
    const int fromIndex = findStop(train.stops, request.departureStationCode);
    const int toIndex = findStop(train.stops, request.arrivalStationCode);
    if (fromIndex < 0 || toIndex <= fromIndex)
        return OperationResult::failure(QStringLiteral("车次不支持该直达区间。"));

    int seatIndex = -1;
    for (int i = 0; i < train.seats.size(); ++i) {
        if (train.seats.at(i).seatType == request.seatType) {
            seatIndex = i;
            break;
        }
    }
    if (seatIndex < 0)
        return OperationResult::failure(QStringLiteral("席别不存在，请重新查询。"));

    domain::SeatInventory &seat = train.seats[seatIndex];
    if (seat.segments.size() < toIndex)
        return OperationResult::failure(QStringLiteral("席别区间配置不完整。"));
    const int passengerCount = passengerIds.size();
    qint64 unitPriceCents = 0;
    for (int segment = fromIndex; segment < toIndex; ++segment) {
        const domain::SegmentInventory &inventory = seat.segments.at(segment);
        if (inventory.remainingSeats < passengerCount)
            return OperationResult::failure(QStringLiteral("余票不足，未生成订单。"));
        unitPriceCents += inventory.priceCents;
    }

    for (const QString &passengerId : passengerIds) {
        const auto it = std::find_if(candidate.passengers.cbegin(), candidate.passengers.cend(),
                                     [&passengerId](const domain::Passenger &passenger) {
                                         return passenger.id == passengerId;
                                     });
        if (it == candidate.passengers.cend())
            return OperationResult::failure(QStringLiteral("乘车人信息已变化，请重新选择。"));
        if (!m_dataStore->canAccessOwner(it->ownerUserId))
            return OperationResult::failure(QStringLiteral("无权使用该乘车人。"));
    }

    for (int segment = fromIndex; segment < toIndex; ++segment)
        seat.segments[segment].remainingSeats -= passengerCount;

    domain::Order order;
    order.ownerUserId = m_dataStore->currentUserId();
    order.id = newId();
    order.createdAt = QDateTime::currentDateTime();
    order.totalAmountCents = unitPriceCents * passengerCount;
    for (const QString &passengerId : passengerIds) {
        domain::Ticket ticket;
        ticket.id = newId();
        ticket.passengerId = passengerId;
        ticket.trainNumber = request.trainNumber;
        ticket.fromStationCode = request.departureStationCode;
        ticket.toStationCode = request.arrivalStationCode;
        ticket.seatType = seat.seatType;
        ticket.priceCents = unitPriceCents;
        ticket.status = domain::TicketStatus::Issued;
        ticket.serviceDate = request.serviceDate;
        ticket.railwayTrainId = request.railwayTrainId;
        candidate.tickets.push_back(ticket);
        order.ticketIds.push_back(ticket.id);
    }
    candidate.orders.push_back(order);

    const OperationResult committed = m_dataStore->commit(std::move(candidate));
    if (!committed)
        return committed;
    if (receipt) {
        receipt->orderId = order.id;
        receipt->ticketIds = order.ticketIds;
        receipt->totalAmountCents = order.totalAmountCents;
    }
    return OperationResult::ok();
}

OperationResult BookingService::bookDemo(const BookingRequest &request,
                                         const DemoTrainSnapshot &snapshot,
                                         BookingReceipt *receipt)
{
    if (!m_dataStore || m_dataStore->currentUserId().isEmpty())
        return OperationResult::failure(QStringLiteral("请先登录。"));
    if (!m_dataStore || snapshot.departureStationName.isEmpty()
        || snapshot.arrivalStationName.isEmpty() || !snapshot.departureTime.isValid()
        || !snapshot.arrivalTime.isValid() || snapshot.priceCents < 0
        || snapshot.remainingSeats <= 0 || request.trainNumber.trimmed().isEmpty()
        || !request.serviceDate.isValid() || request.departureStationCode.isEmpty()
        || request.arrivalStationCode.isEmpty()
        || request.departureStationCode == request.arrivalStationCode
        || request.seatType.trimmed().isEmpty() || request.passengerIds.isEmpty())
        return OperationResult::failure(QStringLiteral("该席别缺少票价或余票，无法生成演示订单。"));

    domain::AppData candidate = m_dataStore->data();
    QStringList passengerIds;
    for (const QString &passengerId : request.passengerIds) {
        if (passengerId.isEmpty() || passengerIds.contains(passengerId))
            return OperationResult::failure(QStringLiteral("乘车人选择重复或无效。"));
        const bool exists = std::any_of(candidate.passengers.cbegin(), candidate.passengers.cend(),
                                        [this, &passengerId](const domain::Passenger &passenger) {
            return passenger.id == passengerId && m_dataStore->canAccessOwner(passenger.ownerUserId);
        });
        if (!exists)
            return OperationResult::failure(QStringLiteral("乘车人信息已变化，请重新选择。"));
        passengerIds.push_back(passengerId);
    }
    if (passengerIds.size() > snapshot.remainingSeats)
        return OperationResult::failure(QStringLiteral("余票不足，未生成订单。"));

    const auto ensureStation = [&candidate](const QString &code, const QString &name) {
        const auto it = std::find_if(candidate.stations.begin(), candidate.stations.end(),
                                     [&code](const domain::Station &station) {
            return station.code == code;
        });
        if (it == candidate.stations.end())
            candidate.stations.push_back({code, name, name, true});
        else {
            it->name = name;
            it->enabled = true;
        }
    };
    if (snapshot.routeStops.isEmpty()) {
        ensureStation(request.departureStationCode, snapshot.departureStationName);
        ensureStation(request.arrivalStationCode, snapshot.arrivalStationName);
    } else {
        for (const DemoTrainSnapshot::RouteStop &stop : snapshot.routeStops)
            ensureStation(stop.code, stop.name);
    }

    domain::Train imported;
    imported.number = request.trainNumber;
    imported.railwayTrainId = request.railwayTrainId;
    imported.railwayServiceDate = request.serviceDate;
    if (snapshot.routeStops.isEmpty()) {
        imported.stops = {{request.departureStationCode, 0, {}, snapshot.departureTime, snapshot.departureDayOffset},
                          {request.arrivalStationCode, 1, snapshot.arrivalTime, {}, snapshot.arrivalDayOffset}};
    } else {
        for (int index = 0; index < snapshot.routeStops.size(); ++index) {
            const DemoTrainSnapshot::RouteStop &source = snapshot.routeStops.at(index);
            imported.stops.append({source.code, index, source.arrivalTime, source.departureTime, source.dayOffset});
        }
    }
    const int importedFrom = findStop(imported.stops, request.departureStationCode);
    const int importedTo = findStop(imported.stops, request.arrivalStationCode);
    if (importedFrom < 0 || importedTo <= importedFrom)
        return OperationResult::failure(QStringLiteral("12306 经停站信息不包含所选购票区间。"));
    QVector<domain::SegmentInventory> segments;
    segments.reserve(imported.stops.size() - 1);
    const int coveredSegments = importedTo - importedFrom;
    for (int index = 0; index < imported.stops.size() - 1; ++index) {
        const bool covered = index >= importedFrom && index < importedTo;
        const qint64 basePrice = covered ? snapshot.priceCents / coveredSegments : 0;
        const qint64 remainder = covered && index == importedTo - 1
                                     ? snapshot.priceCents % coveredSegments : 0;
        segments.append({basePrice + remainder, snapshot.remainingSeats, snapshot.remainingSeats});
    }
    imported.seats = {{request.seatType, segments}};

    const auto trainIt = std::find_if(candidate.trains.begin(), candidate.trains.end(),
                                      [&request](const domain::Train &train) {
        if (request.railwayTrainId.isEmpty())
            return train.number == request.trainNumber && train.railwayTrainId.isEmpty()
                && train.railwayServiceDate == request.serviceDate;
        return train.railwayTrainId == request.railwayTrainId
            && train.railwayServiceDate == request.serviceDate;
    });
    if (trainIt == candidate.trains.end()) {
        candidate.trains.push_back(std::move(imported));
    } else {
        const bool sameRoute = trainIt->stops.size() == imported.stops.size()
            && std::equal(trainIt->stops.cbegin(), trainIt->stops.cend(), imported.stops.cbegin(),
                          [](const domain::TrainStop &left, const domain::TrainStop &right) {
            return left.stationCode.compare(right.stationCode, Qt::CaseInsensitive) == 0;
        });
        if (!sameRoute)
            return OperationResult::failure(QStringLiteral("该车次已保存的经停站与本次 12306 结果不同；为避免错误扣减，请先在管理页隐藏旧记录。"));
        auto seat = std::find_if(trainIt->seats.begin(), trainIt->seats.end(), [&request](const domain::SeatInventory &item) {
            return item.seatType == request.seatType;
        });
        if (seat == trainIt->seats.end()) {
            trainIt->seats.append(imported.seats.first());
        } else {
            if (seat->segments.size() != segments.size())
                return OperationResult::failure(QStringLiteral("该席别的区间库存结构不完整。"));
            for (int index = importedFrom; index < importedTo; ++index) {
                seat->segments[index].priceCents = segments.at(index).priceCents;
                seat->segments[index].totalSeats = std::max(seat->segments.at(index).totalSeats,
                                                             snapshot.remainingSeats);
                seat->segments[index].remainingSeats = std::min(seat->segments.at(index).remainingSeats,
                                                                 snapshot.remainingSeats);
            }
        }
    }

    const OperationResult importedResult = m_dataStore->commit(std::move(candidate));
    return importedResult ? book(request, receipt) : importedResult;
}

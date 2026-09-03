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
        if (train.number == request.trainNumber) {
            trainIndex = i;
            break;
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
    }

    for (int segment = fromIndex; segment < toIndex; ++segment)
        seat.segments[segment].remainingSeats -= passengerCount;

    domain::Order order;
    order.id = newId();
    order.createdAt = QDateTime::currentDateTime();
    order.totalAmountCents = unitPriceCents * passengerCount;
    for (const QString &passengerId : passengerIds) {
        domain::Ticket ticket;
        ticket.id = newId();
        ticket.passengerId = passengerId;
        ticket.trainNumber = train.number;
        ticket.fromStationCode = request.departureStationCode;
        ticket.toStationCode = request.arrivalStationCode;
        ticket.seatType = seat.seatType;
        ticket.priceCents = unitPriceCents;
        ticket.status = domain::TicketStatus::Issued;
        ticket.serviceDate = request.serviceDate;
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
                                        [&passengerId](const domain::Passenger &passenger) {
            return passenger.id == passengerId;
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
    ensureStation(request.departureStationCode, snapshot.departureStationName);
    ensureStation(request.arrivalStationCode, snapshot.arrivalStationName);

    domain::Train imported;
    imported.number = request.trainNumber;
    imported.stops = {{request.departureStationCode,
                       0,
                       {},
                       snapshot.departureTime,
                       snapshot.departureDayOffset},
                      {request.arrivalStationCode,
                       1,
                       snapshot.arrivalTime,
                       {},
                       snapshot.arrivalDayOffset}};
    imported.seats = {{request.seatType,
                       {{snapshot.priceCents,
                         snapshot.remainingSeats,
                         snapshot.remainingSeats}}}};

    const auto trainIt = std::find_if(candidate.trains.begin(), candidate.trains.end(),
                                      [&request](const domain::Train &train) {
        return train.number == request.trainNumber;
    });
    if (trainIt == candidate.trains.end())
        candidate.trains.push_back(std::move(imported));
    else
        *trainIt = std::move(imported);

    const OperationResult importedResult = m_dataStore->commit(std::move(candidate));
    return importedResult ? book(request, receipt) : importedResult;
}

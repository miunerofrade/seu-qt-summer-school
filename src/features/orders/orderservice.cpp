#include "features/orders/orderservice.h"

#include "data/datastore.h"

#include <QHash>
#include <QSet>

#include <algorithm>

namespace {
const domain::Train *findTrain(const domain::AppData &data, const domain::Ticket &ticket)
{
    const auto matches = [&ticket](const domain::Train &train) {
        const bool identityMatches = ticket.railwayTrainId.isEmpty()
            ? train.railwayTrainId.isEmpty()
                && (!ticket.serviceDate.isValid() || train.railwayServiceDate == ticket.serviceDate)
            : train.railwayTrainId == ticket.railwayTrainId
                && train.railwayServiceDate == ticket.serviceDate;
        return identityMatches && (!ticket.railwayTrainId.isEmpty() || train.number == ticket.trainNumber);
    };
    const auto it = std::find_if(data.trains.cbegin(), data.trains.cend(), matches);
    if (it != data.trains.cend())
        return &*it;
    if (ticket.railwayTrainId.isEmpty()) {
        const auto definition = std::find_if(data.trains.cbegin(), data.trains.cend(), [&ticket](const domain::Train &train) {
            return train.railwayTrainId.isEmpty() && !train.railwayServiceDate.isValid()
                && train.number == ticket.trainNumber;
        });
        return definition == data.trains.cend() ? nullptr : &*definition;
    }
    return nullptr;
}

const domain::TrainStop *findStop(const domain::Train &train, const QString &stationCode)
{
    const auto it = std::find_if(train.stops.cbegin(), train.stops.cend(),
                                 [&stationCode](const domain::TrainStop &stop) {
                                     return stop.stationCode == stationCode;
                                 });
    return it == train.stops.cend() ? nullptr : &*it;
}

QDateTime stopDateTime(const domain::Train &train,
                       const domain::TrainStop &stop,
                       const QDate &serviceDate,
                       bool departure)
{
    QTime time = departure ? stop.departureTime : stop.arrivalTime;
    if (!time.isValid())
        time = departure ? stop.arrivalTime : stop.departureTime;
    return time.isValid() ? QDateTime(serviceDate.addDays(stop.dayOffset), time) : QDateTime();
}

domain::TicketStatus aggregateStatus(const QVector<const domain::Ticket *> &tickets)
{
    if (!tickets.isEmpty()
        && std::all_of(tickets.cbegin(), tickets.cend(), [](const domain::Ticket *ticket) {
               return ticket->status == domain::TicketStatus::Refunded;
           })) {
        return domain::TicketStatus::Refunded;
    }
    if (std::any_of(tickets.cbegin(), tickets.cend(), [](const domain::Ticket *ticket) {
            return ticket->status == domain::TicketStatus::Issued;
        })) {
        return domain::TicketStatus::Issued;
    }
    // 一张订单中可能有的票已退、其余票已完成；此时已不存在有效待出行车票。
    return domain::TicketStatus::Completed;
}
} // 命名空间

OrderService::OrderService(DataStore *dataStore)
    : m_dataStore(dataStore)
{
}

QVector<OrderSummary> OrderService::summaries() const
{
    QVector<OrderSummary> result;
    if (!m_dataStore)
        return result;

    const domain::AppData &data = m_dataStore->data();
    QHash<QString, const domain::Ticket *> tickets;
    QHash<QString, QString> passengerNames;
    for (const domain::Ticket &ticket : data.tickets)
        tickets.insert(ticket.id, &ticket);
    for (const domain::Passenger &passenger : data.passengers)
        passengerNames.insert(passenger.id, passenger.name);

    result.reserve(data.orders.size());
    for (const domain::Order &order : data.orders) {
        if (!m_dataStore->canAccessOwner(order.ownerUserId))
            continue;
        OrderSummary row;
        row.ownerUsername = m_dataStore->usernameFor(order.ownerUserId);
        row.orderId = order.id;
        row.createdAt = order.createdAt;
        row.totalAmountCents = order.totalAmountCents;
        QVector<const domain::Ticket *> orderTickets;
        for (const QString &ticketId : order.ticketIds) {
            const domain::Ticket *ticket = tickets.value(ticketId, nullptr);
            if (!ticket)
                continue;
            orderTickets.append(ticket);
            row.passengerNames.append(passengerNames.value(ticket->passengerId, ticket->passengerId));
            if (!row.trainNumbers.contains(ticket->trainNumber))
                row.trainNumbers.append(ticket->trainNumber);
            if (!row.seatTypes.contains(ticket->seatType))
                row.seatTypes.append(ticket->seatType);
            const domain::Train *train = findTrain(data, *ticket);
            const QDate serviceDate = ticket->serviceDate;
            if (!row.serviceDate.isValid() || (serviceDate.isValid() && serviceDate < row.serviceDate))
                row.serviceDate = serviceDate;
        }
        row.status = aggregateStatus(orderTickets);
        result.append(row);
    }
    std::sort(result.begin(), result.end(), [](const OrderSummary &left, const OrderSummary &right) {
        return left.createdAt > right.createdAt;
    });
    return result;
}

QVector<OrderTicketDetail> OrderService::details(const QString &orderId) const
{
    QVector<OrderTicketDetail> result;
    if (!m_dataStore)
        return result;
    const domain::AppData &data = m_dataStore->data();
    const auto orderIt = std::find_if(data.orders.cbegin(), data.orders.cend(),
                                      [&orderId](const domain::Order &order) {
                                          return order.id == orderId;
                                      });
    if (orderIt == data.orders.cend() || !m_dataStore->canAccessOwner(orderIt->ownerUserId))
        return result;

    QHash<QString, QString> passengerNames;
    QHash<QString, QString> stationNames;
    for (const domain::Passenger &passenger : data.passengers)
        passengerNames.insert(passenger.id, passenger.name);
    for (const domain::Station &station : data.stations)
        stationNames.insert(station.code, station.name);

    for (const QString &ticketId : orderIt->ticketIds) {
        const auto ticketIt = std::find_if(data.tickets.cbegin(), data.tickets.cend(),
                                           [&ticketId](const domain::Ticket &ticket) {
                                               return ticket.id == ticketId;
                                           });
        if (ticketIt == data.tickets.cend())
            continue;
        const domain::Train *train = findTrain(data, *ticketIt);
        const domain::TrainStop *fromStop = train ? findStop(*train, ticketIt->fromStationCode) : nullptr;
        const domain::TrainStop *toStop = train ? findStop(*train, ticketIt->toStationCode) : nullptr;
        result.append({ticketIt->id,
                       passengerNames.value(ticketIt->passengerId, ticketIt->passengerId),
                       ticketIt->trainNumber,
                       ticketIt->serviceDate,
                       stationNames.value(ticketIt->fromStationCode, ticketIt->fromStationCode),
                       stationNames.value(ticketIt->toStationCode, ticketIt->toStationCode),
                       train && fromStop ? stopDateTime(*train, *fromStop, ticketIt->serviceDate, true) : QDateTime(),
                       train && toStop ? stopDateTime(*train, *toStop, ticketIt->serviceDate, false) : QDateTime(),
                       ticketIt->seatType,
                       ticketIt->seatId,
                       ticketIt->priceCents,
                       ticketIt->status});
    }
    return result;
}

OperationResult OrderService::refreshCompletedTickets(const QDateTime &now)
{
    if (!m_dataStore || !now.isValid())
        return OperationResult::failure(QStringLiteral("无法刷新车票状态。"));
    domain::AppData candidate = m_dataStore->data();
    bool changed = false;
    for (domain::Ticket &ticket : candidate.tickets) {
        if (ticket.status != domain::TicketStatus::Issued)
            continue;
        const domain::Train *train = findTrain(candidate, ticket);
        const domain::TrainStop *stop = train ? findStop(*train, ticket.toStationCode) : nullptr;
        const QDateTime arrivalAt = train && stop
                                        ? stopDateTime(*train, *stop, ticket.serviceDate, false)
                                        : QDateTime();
        if (arrivalAt.isValid() && now >= arrivalAt) {
            ticket.status = domain::TicketStatus::Completed;
            changed = true;
        }
    }
    return changed ? m_dataStore->commit(std::move(candidate)) : OperationResult::ok();
}

QString OrderService::statusText(domain::TicketStatus status)
{
    switch (status) {
    case domain::TicketStatus::Issued:
        return QStringLiteral("已出票");
    case domain::TicketStatus::Refunded:
        return QStringLiteral("已退票");
    case domain::TicketStatus::Completed:
        return QStringLiteral("已完成");
    }
    return {};
}

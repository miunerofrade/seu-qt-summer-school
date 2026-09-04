#include "features/orders/refundservice.h"

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

int findTrain(const domain::AppData &data, const domain::Ticket &ticket)
{
    for (int i = 0; i < data.trains.size(); ++i) {
        const domain::Train &train = data.trains.at(i);
        const bool identityMatches = ticket.railwayTrainId.isEmpty()
            ? train.railwayTrainId.isEmpty()
                && (!ticket.serviceDate.isValid() || train.railwayServiceDate == ticket.serviceDate)
            : train.railwayTrainId == ticket.railwayTrainId
                && train.railwayServiceDate == ticket.serviceDate;
        if (identityMatches && (!ticket.railwayTrainId.isEmpty() || train.number == ticket.trainNumber)) {
            return i;
        }
    }
    if (ticket.railwayTrainId.isEmpty()) {
        for (int i = 0; i < data.trains.size(); ++i) {
            const domain::Train &train = data.trains.at(i);
            if (train.railwayTrainId.isEmpty() && !train.railwayServiceDate.isValid()
                && train.number == ticket.trainNumber)
                return i;
        }
    }
    return -1;
}

QDateTime departureDateTime(const domain::Train &train,
                            const domain::TrainStop &stop,
                            const QDate &serviceDate)
{
    const QTime time = stop.departureTime.isValid() ? stop.departureTime : stop.arrivalTime;
    return time.isValid() ? QDateTime(serviceDate.addDays(stop.dayOffset), time) : QDateTime();
}

int refundRate(qint64 secondsBeforeDeparture)
{
    constexpr qint64 hour = 60 * 60;
    constexpr qint64 day = 24 * hour;
    if (secondsBeforeDeparture >= 8 * day)
        return 0;
    if (secondsBeforeDeparture >= 48 * hour)
        return 5;
    if (secondsBeforeDeparture >= 24 * hour)
        return 10;
    return 20;
}

qint64 roundedPercent(qint64 cents, int percent)
{
    return (cents * percent + 50) / 100;
}

OperationResult buildQuote(const domain::AppData &data,
                           const QString &ticketId,
                           const QDateTime &now,
                           RefundQuote *result)
{
    if (ticketId.trimmed().isEmpty() || !now.isValid() || !result)
        return OperationResult::failure(QStringLiteral("退票信息不完整。"));
    const auto ticketIt = std::find_if(data.tickets.cbegin(), data.tickets.cend(),
                                       [&ticketId](const domain::Ticket &ticket) {
                                           return ticket.id == ticketId;
                                       });
    if (ticketIt == data.tickets.cend())
        return OperationResult::failure(QStringLiteral("车票不存在。"));
    if (ticketIt->status == domain::TicketStatus::Refunded)
        return OperationResult::failure(QStringLiteral("该车票已经退票，不能重复办理。"));
    if (ticketIt->status == domain::TicketStatus::Completed)
        return OperationResult::failure(QStringLiteral("该车票行程已经完成，不能退票。"));

    const int trainIndex = findTrain(data, *ticketIt);
    if (trainIndex < 0)
        return OperationResult::failure(QStringLiteral("找不到车票对应的车次。"));
    const domain::Train &train = data.trains.at(trainIndex);
    const int fromIndex = findStop(train.stops, ticketIt->fromStationCode);
    const int toIndex = findStop(train.stops, ticketIt->toStationCode);
    if (fromIndex < 0 || toIndex <= fromIndex)
        return OperationResult::failure(QStringLiteral("车票乘车区间无效。"));
    const QDateTime departureAt = departureDateTime(train,
                                                    train.stops.at(fromIndex),
                                                    ticketIt->serviceDate);
    if (!departureAt.isValid())
        return OperationResult::failure(QStringLiteral("车次开车时间无效。"));
    const qint64 secondsBeforeDeparture = now.secsTo(departureAt);
    if (secondsBeforeDeparture <= 0)
        return OperationResult::failure(QStringLiteral("列车已经开车，不能退票。"));

    result->ticketId = ticketIt->id;
    result->departureAt = departureAt;
    result->ratePercent = refundRate(secondsBeforeDeparture);
    result->feeCents = roundedPercent(ticketIt->priceCents, result->ratePercent);
    result->refundAmountCents = ticketIt->priceCents - result->feeCents;
    return OperationResult::ok();
}
} // 命名空间

RefundService::RefundService(DataStore *dataStore)
    : m_dataStore(dataStore)
{
}

OperationResult RefundService::quote(const QString &ticketId,
                                     const QDateTime &now,
                                     RefundQuote *result) const
{
    if (!m_dataStore)
        return OperationResult::failure(QStringLiteral("数据服务不可用。"));
    if (!m_dataStore->canAccessTicket(ticketId))
        return OperationResult::failure(QStringLiteral("无权操作该车票。"));
    return buildQuote(m_dataStore->data(), ticketId, now, result);
}

OperationResult RefundService::refund(const QString &ticketId,
                                      const QDateTime &now,
                                      RefundReceipt *receipt)
{
    if (!m_dataStore)
        return OperationResult::failure(QStringLiteral("数据服务不可用。"));
    if (!m_dataStore->canAccessTicket(ticketId))
        return OperationResult::failure(QStringLiteral("无权操作该车票。"));
    domain::AppData candidate = m_dataStore->data();
    RefundQuote quoteResult;
    const OperationResult quoted = buildQuote(candidate, ticketId, now, &quoteResult);
    if (!quoted)
        return quoted;

    auto ticketIt = std::find_if(candidate.tickets.begin(), candidate.tickets.end(),
                                 [&ticketId](const domain::Ticket &ticket) {
                                     return ticket.id == ticketId;
                                 });
    const int trainIndex = findTrain(candidate, *ticketIt);
    domain::Train &train = candidate.trains[trainIndex];
    const int fromIndex = findStop(train.stops, ticketIt->fromStationCode);
    const int toIndex = findStop(train.stops, ticketIt->toStationCode);
    auto seatIt = std::find_if(train.seats.begin(), train.seats.end(),
                               [&ticketIt](const domain::SeatInventory &seat) {
                                   return seat.seatType == ticketIt->seatType;
                               });
    if (seatIt == train.seats.end() || seatIt->segments.size() < toIndex)
        return OperationResult::failure(QStringLiteral("找不到车票对应的席别区间。"));
    for (int segment = fromIndex; segment < toIndex; ++segment) {
        if (seatIt->segments.at(segment).remainingSeats >= seatIt->segments.at(segment).totalSeats)
            return OperationResult::failure(QStringLiteral("区间余票数据异常，无法恢复余票。"));
    }
    for (int segment = fromIndex; segment < toIndex; ++segment)
        ++seatIt->segments[segment].remainingSeats;

    ticketIt->status = domain::TicketStatus::Refunded;
    domain::RefundRecord record;
    record.id = newId();
    record.ticketId = ticketIt->id;
    record.processedAt = now;
    record.ratePercent = quoteResult.ratePercent;
    record.feeCents = quoteResult.feeCents;
    record.refundAmountCents = quoteResult.refundAmountCents;
    candidate.refunds.append(record);

    const OperationResult committed = m_dataStore->commit(std::move(candidate));
    if (!committed)
        return committed;
    if (receipt) {
        receipt->ticketId = quoteResult.ticketId;
        receipt->departureAt = quoteResult.departureAt;
        receipt->ratePercent = quoteResult.ratePercent;
        receipt->feeCents = quoteResult.feeCents;
        receipt->refundAmountCents = quoteResult.refundAmountCents;
        receipt->refundRecordId = record.id;
    }
    return OperationResult::ok();
}

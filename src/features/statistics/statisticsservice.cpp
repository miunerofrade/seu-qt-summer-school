#include "features/statistics/statisticsservice.h"

#include "data/datastore.h"

#include <QHash>
#include <QMap>

#include <algorithm>

namespace {
struct RowAccumulator
{
    StatisticsRow row;
    int remainingTotal = 0;
    int capacityTotal = 0;
};

const domain::Train *findTrain(const domain::AppData &data, const domain::Ticket &ticket)
{
    const auto it = std::find_if(data.trains.cbegin(), data.trains.cend(),
                                 [&ticket](const domain::Train &train) {
                                     if (!ticket.railwayTrainId.isEmpty())
                                         return train.railwayTrainId == ticket.railwayTrainId
                                             && train.railwayServiceDate == ticket.serviceDate;
                                     return train.railwayTrainId.isEmpty()
                                         && train.number == ticket.trainNumber
                                         && (!ticket.serviceDate.isValid()
                                             || train.railwayServiceDate == ticket.serviceDate);
                                 });
    if (it != data.trains.cend())
        return &*it;
    const auto definition = std::find_if(data.trains.cbegin(), data.trains.cend(), [&ticket](const domain::Train &train) {
        return ticket.railwayTrainId.isEmpty() && train.railwayTrainId.isEmpty()
            && !train.railwayServiceDate.isValid() && train.number == ticket.trainNumber;
    });
    return definition == data.trains.cend() ? nullptr : &*definition;
}

bool dateInRange(const QDate &date, const StatisticsFilter &filter)
{
    return date.isValid() && (!filter.from.isValid() || date >= filter.from)
        && (!filter.to.isValid() || date <= filter.to);
}

bool stationMatches(const domain::Train &train, const QString &stationCode)
{
    if (stationCode.isEmpty())
        return true;
    return std::any_of(train.stops.cbegin(), train.stops.cend(), [&stationCode](const domain::TrainStop &stop) {
        return stop.stationCode == stationCode;
    });
}

QString routeText(const domain::AppData &data, const domain::Ticket &ticket)
{
    QHash<QString, QString> names;
    for (const domain::Station &station : data.stations)
        names.insert(station.code, station.name);
    return QStringLiteral("%1 → %2")
        .arg(names.value(ticket.fromStationCode, ticket.fromStationCode),
             names.value(ticket.toStationCode, ticket.toStationCode));
}
} // 命名空间

StatisticsService::StatisticsService(const DataStore *dataStore)
    : m_dataStore(dataStore)
{
}

StatisticsSummary StatisticsService::summarize(const StatisticsFilter &filter) const
{
    StatisticsSummary summary;
    if (!m_dataStore || (filter.from.isValid() && filter.to.isValid() && filter.from > filter.to))
        return summary;
    const domain::AppData &data = m_dataStore->data();
    QHash<QString, const domain::RefundRecord *> refunds;
    for (const domain::RefundRecord &refund : data.refunds)
        refunds.insert(refund.ticketId, &refund);

    QHash<QString, RowAccumulator> accumulators;
    QMap<QDate, DailyStatisticsPoint> dailyPoints;
    QHash<QString, int> seatSales;
    int remainingTotal = 0;
    int capacityTotal = 0;
    for (const domain::Train &train : data.trains) {
        if ((!filter.trainNumber.isEmpty() && train.number != filter.trainNumber)
            || !stationMatches(train, filter.stationCode)) {
            continue;
        }
        for (const domain::SeatInventory &seat : train.seats) {
            if (!filter.seatType.isEmpty() && seat.seatType != filter.seatType)
                continue;
            for (const domain::SegmentInventory &segment : seat.segments) {
                remainingTotal += segment.remainingSeats;
                capacityTotal += segment.totalSeats;
            }
        }
    }

    for (const domain::Ticket &ticket : data.tickets) {
        const domain::Train *train = findTrain(data, ticket);
        const QDate serviceDate = ticket.serviceDate;
        if (!train || !dateInRange(serviceDate, filter)
            || (!filter.trainNumber.isEmpty() && ticket.trainNumber != filter.trainNumber)
            || (!filter.seatType.isEmpty() && ticket.seatType != filter.seatType)
            || !stationMatches(*train, filter.stationCode)) {
            continue;
        }
        if (!filter.stationCode.isEmpty()
            && ticket.fromStationCode != filter.stationCode && ticket.toStationCode != filter.stationCode) {
            continue;
        }

        const QString key = ticket.trainNumber + QLatin1Char('\n') + routeText(data, ticket);
        RowAccumulator &accumulator = accumulators[key];
        accumulator.row.trainNumber = ticket.trainNumber;
        accumulator.row.route = routeText(data, ticket);
        ++accumulator.row.soldCount;
        accumulator.row.netRevenueCents += ticket.priceCents;
        ++seatSales[ticket.seatType];

        DailyStatisticsPoint &daily = dailyPoints[serviceDate];
        daily.date = serviceDate;
        ++daily.soldCount;
        daily.netRevenueCents += ticket.priceCents;
        if (const auto refund = refunds.value(ticket.id, nullptr)) {
            ++accumulator.row.refundedCount;
            accumulator.row.netRevenueCents -= refund->refundAmountCents;
            ++daily.refundedCount;
            daily.netRevenueCents -= refund->refundAmountCents;
        }
    }

    for (auto it = accumulators.begin(); it != accumulators.end(); ++it) {
        RowAccumulator &accumulator = it.value();
        accumulator.remainingTotal = remainingTotal;
        accumulator.capacityTotal = capacityTotal;
        accumulator.row.remainingRate = capacityTotal > 0
                                            ? static_cast<double>(remainingTotal) / capacityTotal
                                            : 0.0;
        summary.soldCount += accumulator.row.soldCount;
        summary.refundedCount += accumulator.row.refundedCount;
        summary.netRevenueCents += accumulator.row.netRevenueCents;
        summary.rows.append(accumulator.row);
    }
    if (capacityTotal > 0)
        summary.averageRemainingRate = static_cast<double>(remainingTotal) / capacityTotal;

    // 对常用日期范围补零，使趋势线真实表达无销量日期；超长范围只保留有数据日期。
    const qint64 rangeDays = filter.from.isValid() && filter.to.isValid()
                                 ? filter.from.daysTo(filter.to)
                                 : -1;
    if (rangeDays >= 0 && rangeDays <= 366) {
        for (QDate date = filter.from; date <= filter.to; date = date.addDays(1)) {
            DailyStatisticsPoint point = dailyPoints.value(date);
            point.date = date;
            summary.dailyTrend.append(point);
        }
    } else {
        summary.dailyTrend.reserve(dailyPoints.size());
        for (auto it = dailyPoints.cbegin(); it != dailyPoints.cend(); ++it)
            summary.dailyTrend.append(it.value());
    }
    summary.seatShares.reserve(seatSales.size());
    for (auto it = seatSales.cbegin(); it != seatSales.cend(); ++it)
        summary.seatShares.append({it.key(), it.value()});
    std::sort(summary.seatShares.begin(), summary.seatShares.end(), [](const SeatSharePoint &left,
                                                                       const SeatSharePoint &right) {
        return left.soldCount > right.soldCount
            || (left.soldCount == right.soldCount && left.seatType < right.seatType);
    });
    std::sort(summary.rows.begin(), summary.rows.end(), [](const StatisticsRow &left, const StatisticsRow &right) {
        return left.trainNumber < right.trainNumber || (left.trainNumber == right.trainNumber && left.route < right.route);
    });
    return summary;
}

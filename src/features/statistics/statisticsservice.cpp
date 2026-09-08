#include "features/statistics/statisticsservice.h"

#include "data/datastore.h"

#include <QHash>
#include <algorithm>

namespace {
const domain::Train *findTrain(const domain::AppData &data, const domain::Ticket &ticket)
{
    const auto it = std::find_if(data.trains.cbegin(), data.trains.cend(),
                                 [&ticket](const domain::Train &train) {
                                     if (!ticket.railwayTrainId.isEmpty())
                                         return train.railwayTrainId == ticket.railwayTrainId
                                             && train.railwayServiceDate == ticket.serviceDate;
                                     return train.railwayTrainId.isEmpty()
                                         && train.number == ticket.trainNumber
                                         && train.railwayServiceDate == ticket.serviceDate;
                                 });
    if (it != data.trains.cend())
        return &*it;
    const auto definition = std::find_if(data.trains.cbegin(), data.trains.cend(), [&ticket](const domain::Train &train) {
        return ticket.railwayTrainId.isEmpty() && train.railwayTrainId.isEmpty()
            && !train.railwayServiceDate.isValid() && train.number == ticket.trainNumber;
    });
    return definition == data.trains.cend() ? nullptr : &*definition;
}
} // 命名空间

StatisticsService::StatisticsService(const DataStore *dataStore)
    : m_dataStore(dataStore)
{
}

StatisticsSummary StatisticsService::summarize() const
{
    StatisticsSummary summary;
    if (!m_dataStore)
        return summary;
    const domain::AppData &data = m_dataStore->data();
    QHash<QString, const domain::RefundRecord *> refunds;
    for (const domain::RefundRecord &refund : data.refunds)
        refunds.insert(refund.ticketId, &refund);

    int remainingTotal = 0;
    int capacityTotal = 0;
    for (const domain::Train &train : data.trains) {
        for (const domain::SeatInventory &seat : train.seats) {
            for (const domain::SegmentInventory &segment : seat.segments) {
                remainingTotal += segment.remainingSeats;
                capacityTotal += segment.totalSeats;
            }
        }
    }

    for (const domain::Ticket &ticket : data.tickets) {
        const domain::Train *train = findTrain(data, ticket);
        if (!train)
            continue;

        ++summary.soldCount;
        summary.netRevenueCents += ticket.priceCents;
        if (const auto refund = refunds.value(ticket.id, nullptr)) {
            ++summary.refundedCount;
            summary.netRevenueCents -= refund->refundAmountCents;
        }
    }

    if (capacityTotal > 0)
        summary.averageRemainingRate = static_cast<double>(remainingTotal) / capacityTotal;
    return summary;
}

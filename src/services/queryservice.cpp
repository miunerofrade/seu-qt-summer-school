#include "services/queryservice.h"

#include "data/datastore.h"

#include <QHash>

#include <algorithm>
#include <limits>

namespace {
int absoluteMinutes(const QTime &time, int dayOffset)
{
    return time.hour() * 60 + time.minute() + dayOffset * 24 * 60;
}

QString stationName(const QHash<QString, domain::Station> &stations, const QString &code)
{
    const auto it = stations.constFind(code);
    return it == stations.cend() ? code : it->name;
}

bool isEnabledStation(const QHash<QString, domain::Station> &stations, const QString &code)
{
    const auto it = stations.constFind(code);
    return it != stations.cend() && it->enabled;
}
} // namespace

QueryService::QueryService(const DataStore *dataStore)
    : m_dataStore(dataStore)
{
}

QVector<TrainQueryRow> QueryService::query(const TrainQueryRequest &request) const
{
    QVector<TrainQueryRow> rows;
    if (!m_dataStore || request.departureStationCode.isEmpty()
        || request.arrivalStationCode.isEmpty() || request.departureStationCode == request.arrivalStationCode
        || !request.serviceDate.isValid())
        return rows;

    const domain::AppData &data = m_dataStore->data();
    QHash<QString, domain::Station> stations;
    for (const domain::Station &station : data.stations)
        stations.insert(station.code, station);
    if (!isEnabledStation(stations, request.departureStationCode)
        || !isEnabledStation(stations, request.arrivalStationCode))
        return rows;

    for (const domain::Train &train : data.trains) {
        if (!train.enabled || !train.saleOpen || train.serviceDate != request.serviceDate)
            continue;

        int fromIndex = -1;
        int toIndex = -1;
        for (int i = 0; i < train.stops.size(); ++i) {
            if (train.stops.at(i).stationCode == request.departureStationCode)
                fromIndex = i;
            if (train.stops.at(i).stationCode == request.arrivalStationCode)
                toIndex = i;
        }
        if (fromIndex < 0 || toIndex <= fromIndex)
            continue;

        const domain::TrainStop &fromStop = train.stops.at(fromIndex);
        const domain::TrainStop &toStop = train.stops.at(toIndex);
        const QTime departureTime = fromStop.departureTime.isValid() ? fromStop.departureTime
                                                                       : fromStop.arrivalTime;
        const QTime arrivalTime = toStop.arrivalTime.isValid() ? toStop.arrivalTime
                                                                : toStop.departureTime;
        if (!departureTime.isValid() || !arrivalTime.isValid())
            continue;
        const int departureMinutes = absoluteMinutes(fromStop.departureTime.isValid()
                                                         ? fromStop.departureTime
                                                         : fromStop.arrivalTime,
                                                     fromStop.dayOffset);
        int arrivalMinutes = absoluteMinutes(toStop.arrivalTime.isValid() ? toStop.arrivalTime
                                                                            : toStop.departureTime,
                                             toStop.dayOffset);
        while (arrivalMinutes < departureMinutes)
            arrivalMinutes += 24 * 60;

        TrainQueryRow row{train.number,
                          stationName(stations, request.departureStationCode),
                          stationName(stations, request.arrivalStationCode),
                          departureTime,
                          arrivalTime,
                          fromStop.dayOffset,
                          toStop.dayOffset,
                          arrivalMinutes - departureMinutes,
                          {}};
        for (const domain::SeatInventory &seat : train.seats) {
            if (seat.segments.size() < toIndex)
                continue;
            qint64 priceCents = 0;
            int remainingSeats = std::numeric_limits<int>::max();
            bool valid = true;
            for (int segment = fromIndex; segment < toIndex; ++segment) {
                const domain::SegmentInventory &inventory = seat.segments.at(segment);
                if (inventory.priceCents < 0 || inventory.remainingSeats < 0) {
                    valid = false;
                    break;
                }
                priceCents += inventory.priceCents;
                remainingSeats = std::min(remainingSeats, inventory.remainingSeats);
            }
            if (!valid)
                continue;
            row.seats.append({seat.seatType,
                              remainingSeats == std::numeric_limits<int>::max() ? 0 : remainingSeats,
                              priceCents});
        }
        if (!row.seats.isEmpty())
            rows.push_back(std::move(row));
    }
    return rows;
}

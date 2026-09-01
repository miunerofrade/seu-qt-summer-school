#include "services/adminservice.h"

#include "data/datastore.h"

#include <QDateTime>

#include <algorithm>

namespace {
auto findStation(domain::AppData &data, const QString &code)
{
    return std::find_if(data.stations.begin(), data.stations.end(), [&code](const domain::Station &station) {
        return station.code.compare(code, Qt::CaseInsensitive) == 0;
    });
}

auto findTrain(domain::AppData &data, const QString &number)
{
    return std::find_if(data.trains.begin(), data.trains.end(), [&number](const domain::Train &train) {
        return train.number.compare(number, Qt::CaseInsensitive) == 0;
    });
}

OperationResult validateStation(const domain::Station &station)
{
    if (station.code.trimmed().isEmpty())
        return OperationResult::failure(QObject::tr("站码不能为空。"));
    if (station.name.trimmed().isEmpty())
        return OperationResult::failure(QObject::tr("车站名称不能为空。"));
    if (station.city.trimmed().isEmpty())
        return OperationResult::failure(QObject::tr("所在城市不能为空。"));
    return OperationResult::ok();
}
} // namespace

AdminService::AdminService(DataStore *dataStore)
    : m_dataStore(dataStore)
{
}

OperationResult AdminService::addStation(const domain::Station &station)
{
    domain::Station normalized = station;
    normalized.code = normalized.code.trimmed().toUpper();
    normalized.name = normalized.name.trimmed();
    normalized.city = normalized.city.trimmed();
    const OperationResult validation = validateStation(normalized);
    if (!validation)
        return validation;

    domain::AppData candidate = m_dataStore->data();
    if (findStation(candidate, normalized.code) != candidate.stations.end())
        return OperationResult::failure(QObject::tr("站码 %1 已经存在。").arg(normalized.code));
    candidate.stations.append(normalized);
    return m_dataStore->commit(std::move(candidate));
}

OperationResult AdminService::updateStation(const QString &code,
                                            const QString &name,
                                            const QString &city,
                                            bool enabled)
{
    domain::Station normalized{code.trimmed().toUpper(), name.trimmed(), city.trimmed(), enabled};
    const OperationResult validation = validateStation(normalized);
    if (!validation)
        return validation;

    domain::AppData candidate = m_dataStore->data();
    const auto station = findStation(candidate, normalized.code);
    if (station == candidate.stations.end())
        return OperationResult::failure(QObject::tr("所选车站已不存在。"));
    station->name = normalized.name;
    station->city = normalized.city;
    station->enabled = normalized.enabled;
    return m_dataStore->commit(std::move(candidate));
}

OperationResult AdminService::removeStation(const QString &code)
{
    const auto &data = m_dataStore->data();
    const bool usedByTrain = std::any_of(data.trains.cbegin(), data.trains.cend(), [&code](const domain::Train &train) {
        return std::any_of(train.stops.cbegin(), train.stops.cend(), [&code](const domain::TrainStop &stop) {
            return stop.stationCode.compare(code, Qt::CaseInsensitive) == 0;
        });
    });
    const bool usedByTicket = std::any_of(data.tickets.cbegin(), data.tickets.cend(), [&code](const domain::Ticket &ticket) {
        return ticket.fromStationCode.compare(code, Qt::CaseInsensitive) == 0
            || ticket.toStationCode.compare(code, Qt::CaseInsensitive) == 0;
    });
    if (usedByTrain || usedByTicket)
        return OperationResult::failure(QObject::tr("该车站已被车次或车票引用，请改为停用。"));

    domain::AppData candidate = data;
    const auto station = findStation(candidate, code);
    if (station == candidate.stations.end())
        return OperationResult::failure(QObject::tr("所选车站已不存在。"));
    candidate.stations.erase(station);
    return m_dataStore->commit(std::move(candidate));
}

OperationResult AdminService::addTrain(const domain::Train &train)
{
    domain::Train normalized = train;
    normalized.number = normalized.number.trimmed().toUpper();
    if (normalized.number.isEmpty())
        return OperationResult::failure(QObject::tr("车次号不能为空。"));
    if (!normalized.serviceDate.isValid())
        return OperationResult::failure(QObject::tr("运行日期无效。"));

    domain::AppData candidate = m_dataStore->data();
    if (findTrain(candidate, normalized.number) != candidate.trains.end())
        return OperationResult::failure(QObject::tr("车次号 %1 已经存在。").arg(normalized.number));
    candidate.trains.append(normalized);
    return m_dataStore->commit(std::move(candidate));
}

OperationResult AdminService::updateTrain(const QString &number,
                                          const QDate &serviceDate,
                                          bool enabled,
                                          bool saleOpen)
{
    if (!serviceDate.isValid())
        return OperationResult::failure(QObject::tr("运行日期无效。"));
    domain::AppData candidate = m_dataStore->data();
    const auto train = findTrain(candidate, number);
    if (train == candidate.trains.end())
        return OperationResult::failure(QObject::tr("所选车次已不存在。"));
    train->serviceDate = serviceDate;
    train->enabled = enabled;
    train->saleOpen = saleOpen;
    return m_dataStore->commit(std::move(candidate));
}

OperationResult AdminService::removeTrain(const QString &number)
{
    const auto &data = m_dataStore->data();
    const bool referenced = std::any_of(data.tickets.cbegin(), data.tickets.cend(), [&number](const domain::Ticket &ticket) {
        return ticket.trainNumber.compare(number, Qt::CaseInsensitive) == 0;
    });
    if (referenced)
        return OperationResult::failure(QObject::tr("该车次已产生车票，请改为停用。"));

    domain::AppData candidate = data;
    const auto train = findTrain(candidate, number);
    if (train == candidate.trains.end())
        return OperationResult::failure(QObject::tr("所选车次已不存在。"));
    candidate.trains.erase(train);
    return m_dataStore->commit(std::move(candidate));
}

OperationResult AdminService::replaceStops(const QString &trainNumber,
                                           const QVector<domain::TrainStop> &stops)
{
    if (stops.size() < 2)
        return OperationResult::failure(QObject::tr("一趟车至少需要两个经停站。"));

    const auto &data = m_dataStore->data();
    QStringList stationCodes;
    QDateTime previousEvent;
    for (int index = 0; index < stops.size(); ++index) {
        const domain::TrainStop &stop = stops.at(index);
        const auto station = std::find_if(data.stations.cbegin(), data.stations.cend(), [&stop](const domain::Station &item) {
            return item.code.compare(stop.stationCode, Qt::CaseInsensitive) == 0;
        });
        if (station == data.stations.cend())
            return OperationResult::failure(QObject::tr("经停站 %1 不存在。").arg(stop.stationCode));
        if (stationCodes.contains(stop.stationCode, Qt::CaseInsensitive))
            return OperationResult::failure(QObject::tr("同一车次不能重复经过车站 %1。").arg(stop.stationCode));
        stationCodes.append(stop.stationCode);
        if (stop.dayOffset < 0)
            return OperationResult::failure(QObject::tr("跨日偏移不能为负数。"));

        const auto train = std::find_if(data.trains.cbegin(), data.trains.cend(), [&trainNumber](const domain::Train &item) {
            return item.number.compare(trainNumber, Qt::CaseInsensitive) == 0;
        });
        if (train == data.trains.cend())
            return OperationResult::failure(QObject::tr("所选车次已不存在。"));
        const QDate date = train->serviceDate.addDays(stop.dayOffset);
        const QDateTime arrival = stop.arrivalTime.isValid() ? QDateTime(date, stop.arrivalTime) : QDateTime();
        const QDateTime departure = stop.departureTime.isValid() ? QDateTime(date, stop.departureTime) : QDateTime();
        if (arrival.isValid() && departure.isValid() && departure < arrival)
            return OperationResult::failure(QObject::tr("车站 %1 的出发时间早于到达时间。").arg(stop.stationCode));
        const QDateTime firstEvent = arrival.isValid() ? arrival : departure;
        const QDateTime lastEvent = departure.isValid() ? departure : arrival;
        if (!firstEvent.isValid())
            return OperationResult::failure(QObject::tr("车站 %1 至少需要一个到达或出发时间。").arg(stop.stationCode));
        if (previousEvent.isValid() && firstEvent < previousEvent)
            return OperationResult::failure(QObject::tr("经停站时间顺序不合理。"));
        previousEvent = lastEvent;
    }

    domain::AppData candidate = data;
    const auto train = findTrain(candidate, trainNumber);
    if (train == candidate.trains.end())
        return OperationResult::failure(QObject::tr("所选车次已不存在。"));
    train->stops = stops;
    for (int index = 0; index < train->stops.size(); ++index)
        train->stops[index].sequence = index;
    train->seats.clear(); // 区间数量改变后必须重新配置席别票额。
    return m_dataStore->commit(std::move(candidate));
}

OperationResult AdminService::replaceSeats(const QString &trainNumber,
                                           const QVector<domain::SeatInventory> &seats)
{
    const auto &data = m_dataStore->data();
    const auto sourceTrain = std::find_if(data.trains.cbegin(), data.trains.cend(), [&trainNumber](const domain::Train &item) {
        return item.number.compare(trainNumber, Qt::CaseInsensitive) == 0;
    });
    if (sourceTrain == data.trains.cend())
        return OperationResult::failure(QObject::tr("所选车次已不存在。"));
    if (sourceTrain->stops.size() < 2)
        return OperationResult::failure(QObject::tr("请先配置至少两个经停站。"));

    QStringList seatTypes;
    const int segmentCount = sourceTrain->stops.size() - 1;
    for (const domain::SeatInventory &seat : seats) {
        const QString seatType = seat.seatType.trimmed();
        if (seatType.isEmpty())
            return OperationResult::failure(QObject::tr("席别名称不能为空。"));
        if (seatTypes.contains(seatType, Qt::CaseInsensitive))
            return OperationResult::failure(QObject::tr("席别 %1 重复。").arg(seatType));
        seatTypes.append(seatType);
        if (seat.segments.size() != segmentCount)
            return OperationResult::failure(QObject::tr("席别 %1 的区间数量不正确。").arg(seatType));
        for (const domain::SegmentInventory &segment : seat.segments) {
            if (segment.priceCents < 0 || segment.totalSeats < 0 || segment.remainingSeats < 0)
                return OperationResult::failure(QObject::tr("票价和票额不能为负数。"));
            if (segment.remainingSeats > segment.totalSeats)
                return OperationResult::failure(QObject::tr("余票不能大于总票额。"));
        }
    }

    domain::AppData candidate = data;
    const auto train = findTrain(candidate, trainNumber);
    train->seats = seats;
    return m_dataStore->commit(std::move(candidate));
}

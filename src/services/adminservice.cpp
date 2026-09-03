#include "services/adminservice.h"

#include "data/datastore.h"

#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>

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

struct OfficialStationMatch
{
    QString code;
    QString name;
};

OfficialStationMatch officialStationMatch(const DataStore *dataStore,
                                          const QString &code,
                                          const QString &name = {})
{
    QFile file(QDir(dataStore->dataDirectory()).filePath(QStringLiteral("railway-stations.json")));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    const QJsonArray stations = document.object().value(QStringLiteral("stations")).toArray();
    for (const QJsonValue &value : stations) {
        const QJsonObject station = value.toObject();
        const QString officialCode = station.value(QStringLiteral("code")).toString();
        const QString officialName = station.value(QStringLiteral("name")).toString();
        if ((!code.isEmpty() && officialCode.compare(code, Qt::CaseInsensitive) == 0)
            || (!name.isEmpty() && officialName.compare(name, Qt::CaseInsensitive) == 0))
            return {officialCode, officialName};
    }
    return {};
}

QString officialStationName(const DataStore *dataStore, const QString &code)
{
    return officialStationMatch(dataStore, code).name;
}

bool isOfficialStation(const DataStore *dataStore, const QString &code)
{
    return !officialStationName(dataStore, code).isEmpty();
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
    const OfficialStationMatch official = officialStationMatch(m_dataStore,
                                                                normalized.code,
                                                                normalized.name);
    if (!official.code.isEmpty())
        return OperationResult::failure(
            QObject::tr("不能创建：与 12306 官方站点“%1”（站码 %2）重复。官方站点只读且已自动参与查询。")
                .arg(official.name, official.code));

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
    const OfficialStationMatch official = officialStationMatch(m_dataStore,
                                                                normalized.code,
                                                                normalized.name);
    if (!official.code.isEmpty())
        return OperationResult::failure(
            QObject::tr("不能保存：与 12306 官方站点“%1”（站码 %2）重复。")
                .arg(official.name, official.code));

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
    const QString normalized = code.trimmed().toUpper();
    if (isOfficialStation(m_dataStore, normalized))
        return OperationResult::failure(QObject::tr("12306 官方站点只读，不能删除：%1").arg(normalized));
    const auto &data = m_dataStore->data();
    const bool usedByTrain = std::any_of(data.trains.cbegin(), data.trains.cend(), [&normalized](const domain::Train &train) {
        return std::any_of(train.stops.cbegin(), train.stops.cend(), [&normalized](const domain::TrainStop &stop) {
            return stop.stationCode.compare(normalized, Qt::CaseInsensitive) == 0;
        });
    });
    const bool usedByTicket = std::any_of(data.tickets.cbegin(), data.tickets.cend(), [&normalized](const domain::Ticket &ticket) {
        return ticket.fromStationCode.compare(normalized, Qt::CaseInsensitive) == 0
            || ticket.toStationCode.compare(normalized, Qt::CaseInsensitive) == 0;
    });
    if (usedByTrain || usedByTicket)
        return OperationResult::failure(QObject::tr("该车站已被车次或车票引用，请改为停用。"));

    domain::AppData candidate = data;
    const auto station = findStation(candidate, normalized);
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
    if (normalized.stops.size() < 2)
        return OperationResult::failure(QObject::tr("新增车次至少需要起点和终点。"));

    domain::AppData candidate = m_dataStore->data();
    QStringList stationCodes;
    for (int index = 0; index < normalized.stops.size(); ++index) {
        domain::TrainStop &stop = normalized.stops[index];
        stop.stationCode = stop.stationCode.trimmed().toUpper();
        stop.sequence = index;
        if (findStation(candidate, stop.stationCode) == candidate.stations.end()) {
            const QString officialName = officialStationName(m_dataStore, stop.stationCode);
            if (officialName.isEmpty())
                return OperationResult::failure(QObject::tr("经停站 %1 不存在。").arg(stop.stationCode));
            // Keep a read-only local mirror so the existing query and order
            // services can resolve official station names and enabled state.
            candidate.stations.append({stop.stationCode, officialName, officialName, true});
        }
        if (stationCodes.contains(stop.stationCode, Qt::CaseInsensitive))
            return OperationResult::failure(QObject::tr("同一车次不能重复经过车站 %1。").arg(stop.stationCode));
        stationCodes.append(stop.stationCode);
    }
    const auto existing = findTrain(candidate, normalized.number);
    if (existing != candidate.trains.end()
        && !candidate.hiddenTrainNumbers.contains(normalized.number, Qt::CaseInsensitive))
        return OperationResult::failure(QObject::tr("车次号 %1 已经存在。").arg(normalized.number));
    if (existing == candidate.trains.end())
        candidate.trains.append(normalized);
    else
        *existing = normalized;
    candidate.hiddenTrainNumbers.removeAll(normalized.number);
    return m_dataStore->commit(std::move(candidate));
}

OperationResult AdminService::removeTrain(const QString &number, bool knownFromRailwayCache)
{
    const QString normalized = number.trimmed().toUpper();
    if (normalized.isEmpty())
        return OperationResult::failure(QObject::tr("车次号不能为空。"));
    domain::AppData candidate = m_dataStore->data();
    const auto train = findTrain(candidate, normalized);
    if (train == candidate.trains.end() && !knownFromRailwayCache)
        return OperationResult::failure(QObject::tr("车次 %1 不存在，未写入隐藏名单。").arg(normalized));
    if (!candidate.hiddenTrainNumbers.contains(normalized, Qt::CaseInsensitive))
        candidate.hiddenTrainNumbers.append(normalized);
    return m_dataStore->commit(std::move(candidate));
}

OperationResult AdminService::replaceStops(const QString &trainNumber,
                                           const QVector<domain::TrainStop> &stops)
{
    if (stops.size() < 2)
        return OperationResult::failure(QObject::tr("一趟车至少需要两个经停站。"));

    const auto &data = m_dataStore->data();
    QStringList stationCodes;
    int previousMinute = -1;
    for (int index = 0; index < stops.size(); ++index) {
        const domain::TrainStop &stop = stops.at(index);
        const auto station = std::find_if(data.stations.cbegin(), data.stations.cend(), [&stop](const domain::Station &item) {
            return item.code.compare(stop.stationCode, Qt::CaseInsensitive) == 0;
        });
        if (station == data.stations.cend() && !isOfficialStation(m_dataStore, stop.stationCode))
            return OperationResult::failure(QObject::tr("经停站 %1 不存在。").arg(stop.stationCode));
        if (stationCodes.contains(stop.stationCode, Qt::CaseInsensitive))
            return OperationResult::failure(QObject::tr("同一车次不能重复经过车站 %1。").arg(stop.stationCode));
        stationCodes.append(stop.stationCode);
        if (stop.dayOffset < 0)
            return OperationResult::failure(QObject::tr("跨日偏移不能为负数。"));

        const int arrival = stop.arrivalTime.isValid()
                                ? stop.dayOffset * 24 * 60 + stop.arrivalTime.hour() * 60 + stop.arrivalTime.minute()
                                : -1;
        const int departure = stop.departureTime.isValid()
                                  ? stop.dayOffset * 24 * 60 + stop.departureTime.hour() * 60 + stop.departureTime.minute()
                                  : -1;
        if (arrival >= 0 && departure >= 0 && departure < arrival)
            return OperationResult::failure(QObject::tr("车站 %1 的出发时间早于到达时间。").arg(stop.stationCode));
        const int firstEvent = arrival >= 0 ? arrival : departure;
        const int lastEvent = departure >= 0 ? departure : arrival;
        if (firstEvent >= 0 && previousMinute >= 0 && firstEvent < previousMinute)
            return OperationResult::failure(QObject::tr("经停站时间顺序不合理。"));
        if (lastEvent >= 0)
            previousMinute = lastEvent;
    }

    domain::AppData candidate = data;
    for (const domain::TrainStop &stop : stops) {
        if (findStation(candidate, stop.stationCode) != candidate.stations.end())
            continue;
        const QString officialName = officialStationName(m_dataStore, stop.stationCode);
        if (!officialName.isEmpty())
            candidate.stations.append({stop.stationCode, officialName, officialName, true});
    }
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

#include "features/admin/adminservice.h"

#include "data/datastore.h"

#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QSet>

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
} // 命名空间

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
            // 保留一个只读本地镜像，使现有查询和订单服务能够解析官方车站名称及启用状态。
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

OperationResult AdminService::restoreTrain(const QString &number)
{
    const QString normalized = number.trimmed().toUpper();
    if (normalized.isEmpty())
        return OperationResult::failure(QObject::tr("车次号不能为空。"));

    domain::AppData candidate = m_dataStore->data();
    const auto firstHidden = std::find_if(candidate.hiddenTrainNumbers.cbegin(),
                                          candidate.hiddenTrainNumbers.cend(),
                                          [&normalized](const QString &hidden) {
        return hidden.compare(normalized, Qt::CaseInsensitive) == 0;
    });
    if (firstHidden == candidate.hiddenTrainNumbers.cend())
        return OperationResult::failure(QObject::tr("车次 %1 当前没有被隐藏。").arg(normalized));

    candidate.hiddenTrainNumbers.erase(
        std::remove_if(candidate.hiddenTrainNumbers.begin(), candidate.hiddenTrainNumbers.end(),
                       [&normalized](const QString &hidden) {
            return hidden.compare(normalized, Qt::CaseInsensitive) == 0;
        }),
        candidate.hiddenTrainNumbers.end());
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
    const QVector<domain::TrainStop> oldStops = train->stops;
    QVector<domain::SeatInventory> oldSeats = train->seats;
    train->stops = stops;
    for (int index = 0; index < train->stops.size(); ++index)
        train->stops[index].sequence = index;

    const int newSegmentCount = stops.size() - 1;
    const auto stopIndex = [](const QVector<domain::TrainStop> &items, const QString &code) {
        for (int index = 0; index < items.size(); ++index) {
            if (items.at(index).stationCode.compare(code, Qt::CaseInsensitive) == 0)
                return index;
        }
        return -1;
    };
    QVector<QVector<int>> oldSources(newSegmentCount);
    for (int newSegment = 0; newSegment < newSegmentCount; ++newSegment) {
        const int oldFrom = stopIndex(oldStops, stops.at(newSegment).stationCode);
        const int oldTo = stopIndex(oldStops, stops.at(newSegment + 1).stationCode);
        if (oldFrom >= 0 && oldTo > oldFrom) {
            for (int oldSegment = oldFrom; oldSegment < oldTo; ++oldSegment)
                oldSources[newSegment].append(oldSegment);
            continue;
        }
        // 新站插在一个旧区间内：该旧区间的库存占用映射到每个新子区间。
        for (int oldSegment = 0; oldSegment + 1 < oldStops.size(); ++oldSegment) {
            const int newFrom = stopIndex(stops, oldStops.at(oldSegment).stationCode);
            const int newTo = stopIndex(stops, oldStops.at(oldSegment + 1).stationCode);
            if (newFrom >= 0 && newTo > newFrom
                && newSegment >= newFrom && newSegment < newTo) {
                oldSources[newSegment].append(oldSegment);
                break;
            }
        }
    }

    QVector<int> singleSourceUses(std::max(0, static_cast<int>(oldStops.size()) - 1));
    for (const QVector<int> &sources : std::as_const(oldSources))
        if (sources.size() == 1)
            ++singleSourceUses[sources.first()];

    train->seats.clear();
    for (domain::SeatInventory oldSeat : std::as_const(oldSeats)) {
        if (oldSeat.details.isEmpty())
            domain::rebuildSeatDetails(&oldSeat);
        domain::SeatInventory remapped;
        remapped.seatType = oldSeat.seatType;
        remapped.details = oldSeat.details;
        QVector<int> occurrence(singleSourceUses.size());
        for (int newSegment = 0; newSegment < newSegmentCount; ++newSegment) {
            const QVector<int> &sources = oldSources.at(newSegment);
            qint64 priceCents = 0;
            int totalSeats = oldSeat.details.size();
            if (!sources.isEmpty()) {
                totalSeats = 0;
                if (sources.size() == 1 && singleSourceUses.at(sources.first()) > 1) {
                    const int oldSegment = sources.first();
                    const int useCount = singleSourceUses.at(oldSegment);
                    const int useIndex = occurrence[oldSegment]++;
                    const qint64 oldPrice = oldSeat.segments.at(oldSegment).priceCents;
                    priceCents = oldPrice / useCount;
                    if (useIndex == useCount - 1)
                        priceCents += oldPrice % useCount;
                } else {
                    for (const int oldSegment : sources)
                        priceCents += oldSeat.segments.at(oldSegment).priceCents;
                }
                for (const int oldSegment : sources)
                    totalSeats = std::max(totalSeats, oldSeat.segments.at(oldSegment).totalSeats);
            }
            remapped.segments.append({priceCents, totalSeats, totalSeats});
        }
        for (int detailIndex = 0; detailIndex < remapped.details.size(); ++detailIndex) {
            const quint64 oldMask = oldSeat.details.at(detailIndex).occupiedMask;
            quint64 newMask = 0;
            for (int newSegment = 0; newSegment < newSegmentCount; ++newSegment) {
                const bool occupied = std::any_of(oldSources.at(newSegment).cbegin(),
                                                  oldSources.at(newSegment).cend(),
                                                  [oldMask](int oldSegment) {
                    return (oldMask & (quint64(1) << oldSegment)) != 0;
                });
                if (occupied)
                    newMask |= quint64(1) << newSegment;
            }
            remapped.details[detailIndex].occupiedMask = newMask;
        }
        domain::syncRemainingSeats(&remapped);
        train->seats.append(std::move(remapped));
    }
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
    if (segmentCount > 53)
        return OperationResult::failure(QObject::tr("座位掩码最多支持 53 个相邻区间。"));
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

    QVector<domain::SeatInventory> normalizedSeats = seats;
    for (domain::SeatInventory &seat : normalizedSeats)
        domain::rebuildSeatDetails(&seat);
    domain::AppData candidate = data;
    const auto train = findTrain(candidate, trainNumber);
    train->seats = std::move(normalizedSeats);
    return m_dataStore->commit(std::move(candidate));
}

OperationResult AdminService::replaceSeatDetails(
    const QString &trainNumber,
    const QString &seatType,
    const QVector<domain::SeatDetail> &details)
{
    domain::AppData candidate = m_dataStore->data();
    const auto train = findTrain(candidate, trainNumber);
    if (train == candidate.trains.end())
        return OperationResult::failure(QObject::tr("所选车次已不存在。"));
    const auto seat = std::find_if(train->seats.begin(), train->seats.end(), [&seatType](const domain::SeatInventory &item) {
        return item.seatType.compare(seatType, Qt::CaseInsensitive) == 0;
    });
    if (seat == train->seats.end())
        return OperationResult::failure(QObject::tr("所选席别尚未保存，请先保存主表。"));

    int expectedSeats = 0;
    for (const domain::SegmentInventory &segment : std::as_const(seat->segments))
        expectedSeats = std::max(expectedSeats, segment.totalSeats);
    if (details.size() != expectedSeats)
        return OperationResult::failure(QObject::tr("具体席位数量必须与总票额一致。"));
    const quint64 validMask = domain::segmentMask(0, seat->segments.size());
    QSet<QString> ids;
    for (const domain::SeatDetail &detail : details) {
        const QString id = detail.seatId.trimmed();
        if (id.isEmpty() || ids.contains(id.toCaseFolded()))
            return OperationResult::failure(QObject::tr("具体席位编号不能为空或重复。"));
        if ((detail.occupiedMask & ~validMask) != 0)
            return OperationResult::failure(QObject::tr("席位 %1 的掩码包含无效区间位。").arg(id));
        ids.insert(id.toCaseFolded());
    }
    seat->details = details;
    domain::syncRemainingSeats(&*seat);
    for (const domain::SegmentInventory &segment : std::as_const(seat->segments)) {
        if (segment.remainingSeats > segment.totalSeats)
            return OperationResult::failure(
                QObject::tr("具体席位状态产生的余票不能大于该区间总票额。"));
    }
    return m_dataStore->commit(std::move(candidate));
}

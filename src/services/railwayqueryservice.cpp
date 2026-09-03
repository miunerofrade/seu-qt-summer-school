#include "services/railwayqueryservice.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QUrlQuery>

#include <algorithm>

namespace {
constexpr auto UserAgent = "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) "
                           "AppleWebKit/537.36 (KHTML, like Gecko) "
                           "Chrome/140.0.0.0 Safari/537.36";

QNetworkRequest railwayRequest(const QUrl &url, bool ajax = false)
{
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", UserAgent);
    request.setRawHeader("Accept", ajax ? "application/json, text/javascript, */*; q=0.01" : "*/*");
    request.setRawHeader("Referer", "https://kyfw.12306.cn/otn/leftTicket/init?linktypeid=dc");
    if (ajax)
        request.setRawHeader("X-Requested-With", "XMLHttpRequest");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(20000);
    return request;
}

int durationMinutes(const QString &text)
{
    const QStringList parts = text.split(QLatin1Char(':'));
    if (parts.size() != 2)
        return 0;
    bool hoursOk = false;
    bool minutesOk = false;
    const int hours = parts.at(0).toInt(&hoursOk);
    const int minutes = parts.at(1).toInt(&minutesOk);
    return hoursOk && minutesOk ? hours * 60 + minutes : 0;
}

int availabilityCount(const QString &text)
{
    bool ok = false;
    const int count = text.toInt(&ok);
    return ok ? count : (text == QStringLiteral("有") ? 50 : 0);
}

void appendSeat(const QStringList &fields,
                int fieldIndex,
                const QString &name,
                qint64 priceCents,
                QVector<TrainSeatOption> *seats)
{
    if (!seats || fieldIndex >= fields.size())
        return;
    const QString availability = fields.at(fieldIndex).trimmed();
    if (availability.isEmpty() || availability == QStringLiteral("--"))
        return;
    seats->append({name, availabilityCount(availability), priceCents, availability});
}

void applySeatPriceFallbacks(QVector<TrainSeatOption> *seats)
{
    if (!seats)
        return;
    const auto hardSeat = std::find_if(seats->cbegin(), seats->cend(), [](const TrainSeatOption &seat) {
        return seat.seatType == QObject::tr("硬座") && seat.priceCents >= 0;
    });
    if (hardSeat == seats->cend())
        return;
    for (TrainSeatOption &seat : *seats) {
        // 12306 commonly omits a separate W price entry.  On conventional
        // trains standing tickets use the hard-seat fare for the same range.
        if (seat.seatType == QObject::tr("无座") && seat.priceCents < 0)
            seat.priceCents = hardSeat->priceCents;
    }
}

QHash<QChar, qint64> parseSeatPrices(const QString &encoded)
{
    QHash<QChar, qint64> prices;
    for (qsizetype offset = 0; offset + 10 <= encoded.size(); offset += 10) {
        const QString group = encoded.mid(offset, 10);
        bool ok = false;
        const qint64 tenthsOfYuan = group.mid(1, 5).toLongLong(&ok);
        if (!ok || tenthsOfYuan <= 0)
            continue;
        const QChar seatCode = group.at(0);
        const qint64 cents = tenthsOfYuan * 10;
        if (!prices.contains(seatCode) || cents < prices.value(seatCode))
            prices.insert(seatCode, cents);
    }
    return prices;
}

void mergeSeatPrices(const QStringList &fields, int fieldIndex, QHash<QChar, qint64> *prices)
{
    if (!prices || fieldIndex < 0 || fieldIndex >= fields.size())
        return;
    const QHash<QChar, qint64> candidate = parseSeatPrices(fields.at(fieldIndex));
    for (auto it = candidate.cbegin(); it != candidate.cend(); ++it) {
        if (!prices->contains(it.key()))
            prices->insert(it.key(), it.value());
    }
}

qint64 priceForCodes(const QHash<QChar, qint64> &prices, const QString &codes)
{
    for (const QChar code : codes) {
        if (prices.contains(code))
            return prices.value(code);
    }
    return -1;
}

QString cacheKey(const TrainQueryRow &row)
{
    const QString identity = row.railwayTrainId.isEmpty() ? row.trainNumber : row.railwayTrainId;
    return row.serviceDate.toString(Qt::ISODate) + QLatin1Char('|') + identity
           + QLatin1Char('|') + row.departureStationCode + QLatin1Char('|')
           + row.arrivalStationCode;
}

QJsonObject rowToJson(const TrainQueryRow &row)
{
    QJsonArray seats;
    for (const TrainSeatOption &seat : row.seats) {
        seats.append(QJsonObject{{QStringLiteral("seatType"), seat.seatType},
                                 {QStringLiteral("remainingSeats"), seat.remainingSeats},
                                 {QStringLiteral("priceCents"), seat.priceCents},
                                 {QStringLiteral("availability"), seat.availabilityText}});
    }
    return {{QStringLiteral("trainNumber"), row.trainNumber},
            {QStringLiteral("serviceDate"), row.serviceDate.toString(Qt::ISODate)},
            {QStringLiteral("departureStationCode"), row.departureStationCode},
            {QStringLiteral("arrivalStationCode"), row.arrivalStationCode},
            {QStringLiteral("originStationName"), row.originStationName},
            {QStringLiteral("terminalStationName"), row.terminalStationName},
            {QStringLiteral("departureStationName"), row.departureStationName},
            {QStringLiteral("arrivalStationName"), row.arrivalStationName},
            {QStringLiteral("departureTime"), row.departureTime.toString(QStringLiteral("HH:mm"))},
            {QStringLiteral("arrivalTime"), row.arrivalTime.toString(QStringLiteral("HH:mm"))},
            {QStringLiteral("departureDayOffset"), row.departureDayOffset},
            {QStringLiteral("arrivalDayOffset"), row.arrivalDayOffset},
            {QStringLiteral("durationMinutes"), row.durationMinutes},
            {QStringLiteral("railwayTrainId"), row.railwayTrainId},
            {QStringLiteral("seats"), seats}};
}

bool rowFromJson(const QJsonObject &object, TrainQueryRow *row)
{
    if (!row)
        return false;
    row->trainNumber = object.value(QStringLiteral("trainNumber")).toString();
    row->serviceDate = QDate::fromString(object.value(QStringLiteral("serviceDate")).toString(),
                                         Qt::ISODate);
    row->departureStationCode = object.value(QStringLiteral("departureStationCode")).toString();
    row->arrivalStationCode = object.value(QStringLiteral("arrivalStationCode")).toString();
    row->originStationName = object.value(QStringLiteral("originStationName")).toString();
    row->terminalStationName = object.value(QStringLiteral("terminalStationName")).toString();
    row->departureStationName = object.value(QStringLiteral("departureStationName")).toString();
    row->arrivalStationName = object.value(QStringLiteral("arrivalStationName")).toString();
    if (row->originStationName.isEmpty())
        row->originStationName = row->departureStationName;
    if (row->terminalStationName.isEmpty())
        row->terminalStationName = row->arrivalStationName;
    row->departureTime = QTime::fromString(object.value(QStringLiteral("departureTime")).toString(),
                                           QStringLiteral("HH:mm"));
    row->arrivalTime = QTime::fromString(object.value(QStringLiteral("arrivalTime")).toString(),
                                         QStringLiteral("HH:mm"));
    row->departureDayOffset = object.value(QStringLiteral("departureDayOffset")).toInt();
    row->arrivalDayOffset = object.value(QStringLiteral("arrivalDayOffset")).toInt();
    row->durationMinutes = object.value(QStringLiteral("durationMinutes")).toInt();
    row->railwayTrainId = object.value(QStringLiteral("railwayTrainId")).toString();
    row->bookable = false;
    const QJsonArray seats = object.value(QStringLiteral("seats")).toArray();
    for (const QJsonValue &value : seats) {
        const QJsonObject seat = value.toObject();
        row->seats.append({seat.value(QStringLiteral("seatType")).toString(),
                           seat.value(QStringLiteral("remainingSeats")).toInt(),
                           static_cast<qint64>(seat.value(QStringLiteral("priceCents")).toDouble(-1)),
                           seat.value(QStringLiteral("availability")).toString()});
    }
    applySeatPriceFallbacks(&row->seats);
    return !row->trainNumber.isEmpty() && row->serviceDate.isValid()
           && !row->originStationName.isEmpty() && !row->terminalStationName.isEmpty()
           && row->departureTime.isValid() && row->arrivalTime.isValid();
}
} // namespace

RailwayQueryService::RailwayQueryService(const QString &cacheDirectory, QObject *parent)
    : QObject(parent)
    , m_cachePath(QDir(cacheDirectory).filePath(QStringLiteral("railway-cache.json")))
    , m_stationCatalogPath(QDir(cacheDirectory).filePath(QStringLiteral("railway-stations.json")))
    , m_network(new QNetworkAccessManager(this))
{
}

void RailwayQueryService::query(const QString &fromCode,
                                const QString &toCode,
                                const QDate &date,
                                QueryCallback callback)
{
    const QUrl initUrl(QStringLiteral("https://kyfw.12306.cn/otn/leftTicket/init?linktypeid=dc"));
    QNetworkReply *initReply = m_network->get(railwayRequest(initUrl));
    connect(initReply, &QNetworkReply::finished, this,
            [this, initReply, fromCode, toCode, date, callback = std::move(callback)]() mutable {
        if (initReply->error() != QNetworkReply::NoError) {
            const QString error = tr("无法建立12306会话：%1").arg(initReply->errorString());
            initReply->deleteLater();
            callback({}, error);
            return;
        }
        initReply->deleteLater();

        QUrl url(QStringLiteral("https://kyfw.12306.cn/otn/leftTicket/queryG"));
        QUrlQuery parameters;
        parameters.addQueryItem(QStringLiteral("leftTicketDTO.train_date"), date.toString(Qt::ISODate));
        parameters.addQueryItem(QStringLiteral("leftTicketDTO.from_station"), fromCode);
        parameters.addQueryItem(QStringLiteral("leftTicketDTO.to_station"), toCode);
        parameters.addQueryItem(QStringLiteral("purpose_codes"), QStringLiteral("ADULT"));
        url.setQuery(parameters);

        QNetworkReply *reply = m_network->get(railwayRequest(url, true));
        connect(reply, &QNetworkReply::finished, this,
                [this, reply, fromCode, toCode, date, callback = std::move(callback)]() mutable {
            if (reply->error() != QNetworkReply::NoError) {
                const QString error = tr("12306查询失败：%1").arg(reply->errorString());
                reply->deleteLater();
                callback({}, error);
                return;
            }
            const QByteArray payload = reply->readAll();
            reply->deleteLater();
            QVector<TrainQueryRow> rows;
            QString error;
            if (!parseTicketResponse(payload, date, &rows, &error)) {
                callback({}, error);
                return;
            }
            QHash<QString, QString> stationNames;
            for (const RailwayStation &station : cachedStations())
                stationNames.insert(station.code, station.name);
            for (TrainQueryRow &row : rows) {
                row.originStationName = stationNames.value(row.originStationName,
                                                           row.originStationName);
                row.terminalStationName = stationNames.value(row.terminalStationName,
                                                             row.terminalStationName);
            }
            const QDateTime now = QDateTime::currentDateTime();
            rows.erase(std::remove_if(rows.begin(), rows.end(), [&](const TrainQueryRow &row) {
                return QDateTime(row.serviceDate.addDays(row.departureDayOffset),
                                 row.departureTime) <= now;
            }), rows.end());
            saveQuery(fromCode, toCode, date, rows);
            callback(std::move(rows), {});
        });
    });
}

void RailwayQueryService::refreshStations(StationsCallback callback)
{
    const QUrl url(QStringLiteral(
        "https://kyfw.12306.cn/otn/resources/js/framework/station_name.js"));
    QNetworkReply *reply = m_network->get(railwayRequest(url));
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, callback = std::move(callback)]() mutable {
        if (reply->error() != QNetworkReply::NoError) {
            const QString error = tr("站点目录同步失败：%1").arg(reply->errorString());
            reply->deleteLater();
            callback({}, error);
            return;
        }
        const QVector<RailwayStation> stations = parseStationCatalog(reply->readAll());
        reply->deleteLater();
        if (stations.isEmpty()) {
            callback({}, tr("12306返回的站点目录无法解析。"));
            return;
        }
        saveStations(stations);
        callback(stations, {});
    });
}

void RailwayQueryService::queryRoute(const TrainQueryRow &row, RouteCallback callback)
{
    if (row.railwayTrainId.trimmed().isEmpty()) {
        callback({}, tr("该条旧缓存不含 12306 内部车次标识，请重新在线查询后再购票。"));
        return;
    }
    QUrl url(QStringLiteral("https://kyfw.12306.cn/otn/czxx/queryByTrainNo"));
    QUrlQuery parameters;
    parameters.addQueryItem(QStringLiteral("train_no"), row.railwayTrainId);
    parameters.addQueryItem(QStringLiteral("from_station_telecode"), row.departureStationCode);
    parameters.addQueryItem(QStringLiteral("to_station_telecode"), row.arrivalStationCode);
    parameters.addQueryItem(QStringLiteral("depart_date"), row.serviceDate.toString(Qt::ISODate));
    url.setQuery(parameters);
    QNetworkReply *reply = m_network->get(railwayRequest(url, true));
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, row, callback = std::move(callback)]() mutable {
        if (reply->error() != QNetworkReply::NoError) {
            const QVector<RailwayRouteStop> cached = cachedRoute(row);
            const QString message = cached.isEmpty()
                                        ? tr("12306 经停站查询失败：%1").arg(reply->errorString())
                                        : QString();
            reply->deleteLater();
            callback(cached, message);
            return;
        }
        const QByteArray payload = reply->readAll();
        reply->deleteLater();
        QVector<RailwayRouteStop> stops;
        QString error;
        if (!parseRouteResponse(payload, cachedStations(), &stops, &error)) {
            const QVector<RailwayRouteStop> cached = cachedRoute(row);
            callback(cached, cached.isEmpty() ? error : QString());
            return;
        }
        saveRoute(row, stops);
        callback(std::move(stops), {});
    });
}

bool RailwayQueryService::parseTicketResponse(const QByteArray &payload,
                                              const QDate &queryDate,
                                              QVector<TrainQueryRow> *rows,
                                              QString *error)
{
    if (!rows || !error)
        return false;
    rows->clear();
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        *error = QObject::tr("12306返回了非JSON数据。接口可能已变化。");
        return false;
    }
    const QJsonObject root = document.object();
    const QJsonObject data = root.value(QStringLiteral("data")).toObject();
    if (!root.value(QStringLiteral("status")).toBool() || data.isEmpty()) {
        *error = QObject::tr("12306未返回有效查询结果。接口可能已变化。");
        return false;
    }
    QHash<QString, QString> stationNames;
    const QJsonObject map = data.value(QStringLiteral("map")).toObject();
    for (auto it = map.begin(); it != map.end(); ++it)
        stationNames.insert(it.key(), it.value().toString());

    const QJsonArray results = data.value(QStringLiteral("result")).toArray();
    rows->reserve(results.size());
    for (const QJsonValue &value : results) {
        const QStringList fields = value.toString().split(QLatin1Char('|'));
        if (fields.size() < 34)
            continue;
        const QTime departure = QTime::fromString(fields.at(8), QStringLiteral("HH:mm"));
        const QTime arrival = QTime::fromString(fields.at(9), QStringLiteral("HH:mm"));
        if (!departure.isValid() || !arrival.isValid())
            continue;

        TrainQueryRow row;
        row.railwayTrainId = fields.at(2);
        row.trainNumber = fields.at(3);
        row.serviceDate = queryDate;
        row.departureStationCode = fields.at(6);
        row.arrivalStationCode = fields.at(7);
        row.originStationName = fields.at(4);
        row.terminalStationName = fields.at(5);
        row.departureStationName = stationNames.value(row.departureStationCode,
                                                       row.departureStationCode);
        row.arrivalStationName = stationNames.value(row.arrivalStationCode,
                                                     row.arrivalStationCode);
        row.departureTime = departure;
        row.arrivalTime = arrival;
        row.durationMinutes = durationMinutes(fields.at(10));
        const int departureMinute = departure.hour() * 60 + departure.minute();
        row.arrivalDayOffset = (departureMinute + row.durationMinutes) / (24 * 60);
        row.bookable = false;
        QHash<QChar, qint64> prices;
        // Different generations of the page have placed fare groups in
        // yp_info, yp_ex or yp_info_new. Prefer the newest field, then fill
        // missing conventional-seat codes from the older representations.
        mergeSeatPrices(fields, 39, &prices);
        mergeSeatPrices(fields, 34, &prices);
        mergeSeatPrices(fields, 12, &prices);
        appendSeat(fields, 32, QObject::tr("商务座"), priceForCodes(prices, QStringLiteral("9")), &row.seats);
        appendSeat(fields, 25, QObject::tr("特等座"), priceForCodes(prices, QStringLiteral("P")), &row.seats);
        appendSeat(fields, 31, QObject::tr("一等座"), priceForCodes(prices, QStringLiteral("M")), &row.seats);
        appendSeat(fields, 30, QObject::tr("二等座"), priceForCodes(prices, QStringLiteral("OS")), &row.seats);
        appendSeat(fields, 21, QObject::tr("高级软卧"), priceForCodes(prices, QStringLiteral("6A")), &row.seats);
        appendSeat(fields, 23, QObject::tr("软卧"), priceForCodes(prices, QStringLiteral("4IF")), &row.seats);
        appendSeat(fields, 33, QObject::tr("动卧"), priceForCodes(prices, QStringLiteral("FI")), &row.seats);
        appendSeat(fields, 28, QObject::tr("硬卧"), priceForCodes(prices, QStringLiteral("3J")), &row.seats);
        appendSeat(fields, 24, QObject::tr("软座"), priceForCodes(prices, QStringLiteral("2")), &row.seats);
        appendSeat(fields, 29, QObject::tr("硬座"), priceForCodes(prices, QStringLiteral("1")), &row.seats);
        appendSeat(fields, 26, QObject::tr("无座"), priceForCodes(prices, QStringLiteral("W1")), &row.seats);
        applySeatPriceFallbacks(&row.seats);
        if (row.seats.isEmpty())
            row.seats.append({QObject::tr("席位"), 0, -1, QStringLiteral("—")});
        rows->push_back(std::move(row));
    }
    if (!results.isEmpty() && rows->isEmpty()) {
        *error = QObject::tr("12306 已更改余票返回格式，当前响应无法解析；请使用已保存缓存或自定义车次进行课程演示。");
        return false;
    }
    error->clear();
    return true;
}

QVector<RailwayStation> RailwayQueryService::parseStationCatalog(const QByteArray &payload)
{
    QVector<RailwayStation> stations;
    const QString text = QString::fromUtf8(payload);
    const QStringList records = text.split(QLatin1Char('@'));
    for (const QString &record : records) {
        const QStringList fields = record.split(QLatin1Char('|'));
        if (fields.size() < 3 || fields.at(1).trimmed().isEmpty()
            || fields.at(2).trimmed().isEmpty())
            continue;
        stations.push_back({fields.at(1).trimmed(), fields.at(2).trimmed()});
    }
    std::sort(stations.begin(), stations.end(), [](const RailwayStation &left,
                                                    const RailwayStation &right) {
        return left.name.localeAwareCompare(right.name) < 0;
    });
    return stations;
}

bool RailwayQueryService::parseRouteResponse(const QByteArray &payload,
                                             const QVector<RailwayStation> &stations,
                                             QVector<RailwayRouteStop> *stops,
                                             QString *error)
{
    if (!stops || !error)
        return false;
    stops->clear();
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    const QJsonObject root = document.object();
    const QJsonArray rows = root.value(QStringLiteral("data")).toObject()
                                .value(QStringLiteral("data")).toArray();
    if (parseError.error != QJsonParseError::NoError || !root.value(QStringLiteral("status")).toBool()
        || rows.size() < 2) {
        *error = QObject::tr("12306 未返回有效经停站信息。");
        return false;
    }

    int dayOffset = 0;
    int previousMinute = -1;
    for (const QJsonValue &value : rows) {
        const QJsonObject item = value.toObject();
        RailwayRouteStop stop;
        stop.name = item.value(QStringLiteral("station_name")).toString().trimmed();
        const auto station = std::find_if(stations.cbegin(), stations.cend(), [&stop](const RailwayStation &candidate) {
            return candidate.name == stop.name;
        });
        if (station == stations.cend()) {
            *error = QObject::tr("经停站“%1”不在本地 12306 站点目录中，请先同步站点。").arg(stop.name);
            stops->clear();
            return false;
        }
        stop.code = station->code;
        const QString arrivalText = item.value(QStringLiteral("arrive_time")).toString();
        const QString departureText = item.value(QStringLiteral("start_time")).toString();
        stop.arrivalTime = QTime::fromString(arrivalText, QStringLiteral("HH:mm"));
        stop.departureTime = QTime::fromString(departureText, QStringLiteral("HH:mm"));
        const QTime event = stop.arrivalTime.isValid() ? stop.arrivalTime : stop.departureTime;
        if (!event.isValid()) {
            *error = QObject::tr("12306 经停站时刻格式无效。");
            stops->clear();
            return false;
        }
        int minute = event.hour() * 60 + event.minute();
        if (previousMinute >= 0 && minute < previousMinute)
            ++dayOffset;
        stop.dayOffset = dayOffset;
        previousMinute = minute;
        stops->append(std::move(stop));
    }
    error->clear();
    return true;
}

QByteArray RailwayQueryService::cacheContents() const
{
    QFile file(m_cachePath);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

bool RailwayQueryService::updateCache(const std::function<void(QJsonObject *)> &update) const
{
    QJsonObject root = QJsonDocument::fromJson(cacheContents()).object();
    root.insert(QStringLiteral("schemaVersion"), 1);
    update(&root);
    QSaveFile file(m_cachePath);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return file.commit();
}

void RailwayQueryService::saveQuery(const QString &fromCode,
                                    const QString &toCode,
                                    const QDate &date,
                                    const QVector<TrainQueryRow> &rows) const
{
    updateCache([&](QJsonObject *root) {
        QJsonArray queries = root->value(QStringLiteral("queries")).toArray();
        QJsonArray updated;
        const QString dateText = date.toString(Qt::ISODate);
        for (const QJsonValue &value : queries) {
            const QJsonObject item = value.toObject();
            if (item.value(QStringLiteral("fromCode")).toString() == fromCode
                && item.value(QStringLiteral("toCode")).toString() == toCode
                && item.value(QStringLiteral("date")).toString() == dateText)
                continue;
            updated.append(item);
        }
        QJsonArray serializedRows;
        for (const TrainQueryRow &row : rows)
            serializedRows.append(rowToJson(row));
        updated.append(QJsonObject{{QStringLiteral("fromCode"), fromCode},
                                   {QStringLiteral("toCode"), toCode},
                                   {QStringLiteral("date"), dateText},
                                   {QStringLiteral("fetchedAt"), QDateTime::currentDateTime().toString(Qt::ISODate)},
                                   {QStringLiteral("rows"), serializedRows}});
        while (updated.size() > 30)
            updated.removeFirst();
        root->insert(QStringLiteral("queries"), updated);
    });
}

void RailwayQueryService::saveStations(const QVector<RailwayStation> &stations) const
{
    updateCache([&](QJsonObject *root) {
        QJsonArray array;
        for (const RailwayStation &station : stations) {
            array.append(QJsonObject{{QStringLiteral("name"), station.name},
                                     {QStringLiteral("code"), station.code}});
        }
        root->insert(QStringLiteral("stations"), array);
        root->insert(QStringLiteral("stationsFetchedAt"),
                     QDateTime::currentDateTime().toString(Qt::ISODate));
    });
}

void RailwayQueryService::saveRoute(const TrainQueryRow &row,
                                    const QVector<RailwayRouteStop> &stops) const
{
    updateCache([&](QJsonObject *root) {
        QJsonArray routes = root->value(QStringLiteral("routes")).toArray();
        QJsonArray updated;
        for (const QJsonValue &value : routes) {
            const QJsonObject item = value.toObject();
            if (item.value(QStringLiteral("railwayTrainId")).toString() == row.railwayTrainId
                && item.value(QStringLiteral("date")).toString() == row.serviceDate.toString(Qt::ISODate))
                continue;
            updated.append(item);
        }
        QJsonArray serializedStops;
        for (const RailwayRouteStop &stop : stops) {
            serializedStops.append(QJsonObject{{QStringLiteral("name"), stop.name},
                                               {QStringLiteral("code"), stop.code},
                                               {QStringLiteral("arrivalTime"), stop.arrivalTime.toString(QStringLiteral("HH:mm"))},
                                               {QStringLiteral("departureTime"), stop.departureTime.toString(QStringLiteral("HH:mm"))},
                                               {QStringLiteral("dayOffset"), stop.dayOffset}});
        }
        updated.append(QJsonObject{{QStringLiteral("railwayTrainId"), row.railwayTrainId},
                                   {QStringLiteral("date"), row.serviceDate.toString(Qt::ISODate)},
                                   {QStringLiteral("fetchedAt"), QDateTime::currentDateTime().toString(Qt::ISODate)},
                                   {QStringLiteral("stops"), serializedStops}});
        while (updated.size() > 100)
            updated.removeFirst();
        root->insert(QStringLiteral("routes"), updated);
    });
}

QVector<RailwayRouteStop> RailwayQueryService::cachedRoute(const TrainQueryRow &row) const
{
    const QJsonArray routes = QJsonDocument::fromJson(cacheContents()).object()
                                  .value(QStringLiteral("routes")).toArray();
    for (const QJsonValue &value : routes) {
        const QJsonObject item = value.toObject();
        if (item.value(QStringLiteral("railwayTrainId")).toString() != row.railwayTrainId
            || item.value(QStringLiteral("date")).toString() != row.serviceDate.toString(Qt::ISODate))
            continue;
        QVector<RailwayRouteStop> stops;
        for (const QJsonValue &stopValue : item.value(QStringLiteral("stops")).toArray()) {
            const QJsonObject object = stopValue.toObject();
            stops.append({object.value(QStringLiteral("name")).toString(),
                          object.value(QStringLiteral("code")).toString(),
                          QTime::fromString(object.value(QStringLiteral("arrivalTime")).toString(), QStringLiteral("HH:mm")),
                          QTime::fromString(object.value(QStringLiteral("departureTime")).toString(), QStringLiteral("HH:mm")),
                          object.value(QStringLiteral("dayOffset")).toInt()});
        }
        return stops;
    }
    return {};
}

QVector<RailwayStation> RailwayQueryService::cachedStations() const
{
    QVector<RailwayStation> stations;
    QJsonArray array = QJsonDocument::fromJson(cacheContents()).object()
                           .value(QStringLiteral("stations")).toArray();
    if (array.isEmpty()) {
        QFile catalog(m_stationCatalogPath);
        if (catalog.open(QIODevice::ReadOnly)) {
            array = QJsonDocument::fromJson(catalog.readAll()).object()
                        .value(QStringLiteral("stations")).toArray();
        }
    }
    stations.reserve(array.size());
    for (const QJsonValue &value : array) {
        const QJsonObject item = value.toObject();
        const QString name = item.value(QStringLiteral("name")).toString();
        const QString code = item.value(QStringLiteral("code")).toString();
        if (!name.isEmpty() && !code.isEmpty())
            stations.push_back({name, code});
    }
    return stations;
}

QVector<TrainQueryRow> RailwayQueryService::cachedAvailable(const QDateTime &now) const
{
    QVector<TrainQueryRow> rows;
    QHash<QString, int> keys;
    const QJsonArray queries = QJsonDocument::fromJson(cacheContents()).object()
                                   .value(QStringLiteral("queries")).toArray();
    for (const QJsonValue &value : queries) {
        const QJsonObject item = value.toObject();
        const QJsonArray cachedRows = item.value(QStringLiteral("rows")).toArray();
        for (const QJsonValue &cachedRow : cachedRows) {
            TrainQueryRow row;
            if (!rowFromJson(cachedRow.toObject(), &row))
                continue;
            if (QDateTime(row.serviceDate.addDays(row.departureDayOffset), row.departureTime) <= now)
                continue;
            const QString key = cacheKey(row);
            if (keys.contains(key))
                rows[keys.value(key)] = std::move(row);
            else {
                keys.insert(key, rows.size());
                rows.push_back(std::move(row));
            }
        }
    }
    return rows;
}

QVector<TrainQueryRow> RailwayQueryService::cachedAll() const
{
    QVector<TrainQueryRow> rows;
    QHash<QString, int> keys;
    const QJsonArray queries = QJsonDocument::fromJson(cacheContents()).object()
                                   .value(QStringLiteral("queries")).toArray();
    for (const QJsonValue &value : queries) {
        const QJsonArray cachedRows = value.toObject().value(QStringLiteral("rows")).toArray();
        for (const QJsonValue &cachedRow : cachedRows) {
            TrainQueryRow row;
            if (!rowFromJson(cachedRow.toObject(), &row))
                continue;
            const QString key = row.trainNumber.toUpper();
            if (keys.contains(key))
                rows[keys.value(key)] = std::move(row);
            else {
                keys.insert(key, rows.size());
                rows.push_back(std::move(row));
            }
        }
    }
    return rows;
}

QVector<TrainQueryRow> RailwayQueryService::cachedQuery(const QString &fromCode,
                                                        const QString &toCode,
                                                        const QDate &date,
                                                        const QDateTime &now) const
{
    QVector<TrainQueryRow> rows;
    const QJsonArray queries = QJsonDocument::fromJson(cacheContents()).object()
                                   .value(QStringLiteral("queries")).toArray();
    const QString dateText = date.toString(Qt::ISODate);
    for (qsizetype index = queries.size() - 1; index >= 0; --index) {
        const QJsonObject item = queries.at(index).toObject();
        if (item.value(QStringLiteral("fromCode")).toString() != fromCode
            || item.value(QStringLiteral("toCode")).toString() != toCode
            || item.value(QStringLiteral("date")).toString() != dateText)
            continue;
        const QJsonArray cachedRows = item.value(QStringLiteral("rows")).toArray();
        for (const QJsonValue &cachedRow : cachedRows) {
            TrainQueryRow row;
            if (rowFromJson(cachedRow.toObject(), &row)
                && QDateTime(row.serviceDate.addDays(row.departureDayOffset),
                             row.departureTime) > now)
                rows.push_back(std::move(row));
        }
        break;
    }
    return rows;
}

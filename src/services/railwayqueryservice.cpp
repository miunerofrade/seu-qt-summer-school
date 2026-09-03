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
    return ok ? count : (text == QStringLiteral("有") ? 1 : 0);
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
    return row.serviceDate.toString(Qt::ISODate) + QLatin1Char('|') + row.trainNumber
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
    row->departureTime = QTime::fromString(object.value(QStringLiteral("departureTime")).toString(),
                                           QStringLiteral("HH:mm"));
    row->arrivalTime = QTime::fromString(object.value(QStringLiteral("arrivalTime")).toString(),
                                         QStringLiteral("HH:mm"));
    row->departureDayOffset = object.value(QStringLiteral("departureDayOffset")).toInt();
    row->arrivalDayOffset = object.value(QStringLiteral("arrivalDayOffset")).toInt();
    row->durationMinutes = object.value(QStringLiteral("durationMinutes")).toInt();
    row->bookable = false;
    const QJsonArray seats = object.value(QStringLiteral("seats")).toArray();
    for (const QJsonValue &value : seats) {
        const QJsonObject seat = value.toObject();
        row->seats.append({seat.value(QStringLiteral("seatType")).toString(),
                           seat.value(QStringLiteral("remainingSeats")).toInt(),
                           static_cast<qint64>(seat.value(QStringLiteral("priceCents")).toDouble(-1)),
                           seat.value(QStringLiteral("availability")).toString()});
    }
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

        QUrl url(QStringLiteral("https://kyfw.12306.cn/otn/leftTicket/query"));
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
        const QHash<QChar, qint64> prices = parseSeatPrices(fields.size() > 39 ? fields.at(39)
                                                                              : QString());
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
        appendSeat(fields, 26, QObject::tr("无座"), priceForCodes(prices, QStringLiteral("W")), &row.seats);
        if (row.seats.isEmpty())
            row.seats.append({QObject::tr("席位"), 0, -1, QStringLiteral("—")});
        rows->push_back(std::move(row));
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

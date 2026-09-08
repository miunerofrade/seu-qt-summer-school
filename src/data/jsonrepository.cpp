#include "data/jsonrepository.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {
using namespace domain;

bool readString(const QJsonObject &object, const char *key, QString *value, QString *error)
{
    const QJsonValue field = object.value(QLatin1String(key));
    if (!field.isString()) {
        *error = QObject::tr("字段 %1 应为文本。", key);
        return false;
    }
    *value = field.toString();
    return true;
}

bool readBool(const QJsonObject &object, const char *key, bool *value, QString *error)
{
    const QJsonValue field = object.value(QLatin1String(key));
    if (!field.isBool()) {
        *error = QObject::tr("字段 %1 应为布尔值。", key);
        return false;
    }
    *value = field.toBool();
    return true;
}

bool readInteger(const QJsonObject &object, const char *key, qint64 *value, QString *error)
{
    const QJsonValue field = object.value(QLatin1String(key));
    if (!field.isDouble() || !std::isfinite(field.toDouble())
        || std::floor(field.toDouble()) != field.toDouble()) {
        *error = QObject::tr("字段 %1 应为整数。", key);
        return false;
    }
    *value = static_cast<qint64>(field.toDouble());
    return true;
}

bool readArray(const QJsonObject &object, const char *key, QJsonArray *value, QString *error)
{
    const QJsonValue field = object.value(QLatin1String(key));
    if (!field.isArray()) {
        *error = QObject::tr("字段 %1 应为数组。", key);
        return false;
    }
    *value = field.toArray();
    return true;
}

QJsonObject stationToJson(const Station &station)
{
    return {{QStringLiteral("code"), station.code},
            {QStringLiteral("name"), station.name},
            {QStringLiteral("city"), station.city},
            {QStringLiteral("enabled"), station.enabled}};
}

QJsonObject stopToJson(const TrainStop &stop)
{
    return {{QStringLiteral("stationCode"), stop.stationCode},
            {QStringLiteral("sequence"), stop.sequence},
            {QStringLiteral("arrivalTime"), stop.arrivalTime.isValid() ? stop.arrivalTime.toString(Qt::ISODate) : QString()},
            {QStringLiteral("departureTime"), stop.departureTime.isValid() ? stop.departureTime.toString(Qt::ISODate) : QString()},
            {QStringLiteral("dayOffset"), stop.dayOffset}};
}

QJsonObject segmentToJson(const SegmentInventory &segment)
{
    return {{QStringLiteral("priceCents"), static_cast<double>(segment.priceCents)},
            {QStringLiteral("totalSeats"), segment.totalSeats},
            {QStringLiteral("remainingSeats"), segment.remainingSeats}};
}

QJsonObject seatToJson(const SeatInventory &seat)
{
    QJsonArray segments;
    for (const SegmentInventory &segment : seat.segments)
        segments.append(segmentToJson(segment));
    QJsonArray details;
    for (const SeatDetail &detail : seat.details) {
        details.append(QJsonObject{{QStringLiteral("seatId"), detail.seatId},
                                   {QStringLiteral("occupiedMask"), static_cast<double>(detail.occupiedMask)}});
    }
    return {{QStringLiteral("seatType"), seat.seatType},
            {QStringLiteral("segments"), segments},
            {QStringLiteral("details"), details}};
}

QJsonObject trainToJson(const Train &train)
{
    QJsonArray stops;
    for (const TrainStop &stop : train.stops)
        stops.append(stopToJson(stop));
    QJsonArray seats;
    for (const SeatInventory &seat : train.seats)
        seats.append(seatToJson(seat));
    QJsonObject object{{QStringLiteral("number"), train.number},
                       {QStringLiteral("stops"), stops},
                       {QStringLiteral("seats"), seats}};
    if (!train.railwayTrainId.isEmpty())
        object.insert(QStringLiteral("railwayTrainId"), train.railwayTrainId);
    if (train.railwayServiceDate.isValid())
        object.insert(QStringLiteral("railwayServiceDate"), train.railwayServiceDate.toString(Qt::ISODate));
    return object;
}

QJsonObject passengerToJson(const Passenger &passenger)
{
    return {{QStringLiteral("id"), passenger.id},
            {QStringLiteral("ownerUserId"), passenger.ownerUserId},
            {QStringLiteral("name"), passenger.name},
            {QStringLiteral("documentType"), passenger.documentType},
            {QStringLiteral("documentNumber"), passenger.documentNumber}};
}

QJsonObject orderToJson(const Order &order)
{
    QJsonArray ticketIds;
    for (const QString &ticketId : order.ticketIds)
        ticketIds.append(ticketId);
    return {{QStringLiteral("id"), order.id},
            {QStringLiteral("ownerUserId"), order.ownerUserId},
            {QStringLiteral("createdAt"), order.createdAt.toString(Qt::ISODate)},
            {QStringLiteral("ticketIds"), ticketIds},
            {QStringLiteral("totalAmountCents"), static_cast<double>(order.totalAmountCents)}};
}

QJsonObject ticketToJson(const Ticket &ticket)
{
    QJsonObject object{{QStringLiteral("id"), ticket.id},
                       {QStringLiteral("passengerId"), ticket.passengerId},
                       {QStringLiteral("trainNumber"), ticket.trainNumber},
                       {QStringLiteral("fromStationCode"), ticket.fromStationCode},
                       {QStringLiteral("toStationCode"), ticket.toStationCode},
                       {QStringLiteral("seatType"), ticket.seatType},
                       {QStringLiteral("priceCents"), static_cast<double>(ticket.priceCents)},
                       {QStringLiteral("status"), ticketStatusKey(ticket.status)}};
    if (ticket.serviceDate.isValid())
        object.insert(QStringLiteral("serviceDate"), ticket.serviceDate.toString(Qt::ISODate));
    if (!ticket.railwayTrainId.isEmpty())
        object.insert(QStringLiteral("railwayTrainId"), ticket.railwayTrainId);
    if (!ticket.seatId.isEmpty())
        object.insert(QStringLiteral("seatId"), ticket.seatId);
    return object;
}

QJsonObject refundToJson(const RefundRecord &refund)
{
    return {{QStringLiteral("id"), refund.id},
            {QStringLiteral("ticketId"), refund.ticketId},
            {QStringLiteral("processedAt"), refund.processedAt.toString(Qt::ISODate)},
            {QStringLiteral("ratePercent"), refund.ratePercent},
            {QStringLiteral("feeCents"), static_cast<double>(refund.feeCents)},
            {QStringLiteral("refundAmountCents"), static_cast<double>(refund.refundAmountCents)}};
}

QJsonObject appDataToJson(const AppData &data)
{
    QJsonArray users;
    for (const User &user : data.users)
        users.append(QJsonObject{{QStringLiteral("id"), user.id},
                                 {QStringLiteral("username"), user.username},
                                 {QStringLiteral("password"), user.password},
                                 {QStringLiteral("role"), user.role}});
    QJsonArray stations;
    for (const Station &station : data.stations)
        stations.append(stationToJson(station));
    QJsonArray trains;
    for (const Train &train : data.trains)
        trains.append(trainToJson(train));
    QJsonArray passengers;
    for (const Passenger &passenger : data.passengers)
        passengers.append(passengerToJson(passenger));
    QJsonArray orders;
    for (const Order &order : data.orders)
        orders.append(orderToJson(order));
    QJsonArray tickets;
    for (const Ticket &ticket : data.tickets)
        tickets.append(ticketToJson(ticket));
    QJsonArray refunds;
    for (const RefundRecord &refund : data.refunds)
        refunds.append(refundToJson(refund));
    QJsonArray hiddenTrainNumbers;
    for (const QString &number : data.hiddenTrainNumbers)
        hiddenTrainNumbers.append(number);

    return {{QStringLiteral("schemaVersion"), data.schemaVersion},
            {QStringLiteral("users"), users},
            {QStringLiteral("stations"), stations},
            {QStringLiteral("trains"), trains},
            {QStringLiteral("passengers"), passengers},
            {QStringLiteral("orders"), orders},
            {QStringLiteral("tickets"), tickets},
            {QStringLiteral("refunds"), refunds},
            {QStringLiteral("hiddenTrainNumbers"), hiddenTrainNumbers}};
}

bool parseStation(const QJsonValue &value, Station *station, QString *error)
{
    if (!value.isObject()) {
        *error = QObject::tr("车站记录格式错误。");
        return false;
    }
    const QJsonObject object = value.toObject();
    return readString(object, "code", &station->code, error)
        && readString(object, "name", &station->name, error)
        && readString(object, "city", &station->city, error)
        && readBool(object, "enabled", &station->enabled, error);
}

bool parseStop(const QJsonValue &value, TrainStop *stop, QString *error)
{
    if (!value.isObject()) {
        *error = QObject::tr("经停站记录格式错误。");
        return false;
    }
    const QJsonObject object = value.toObject();
    qint64 sequence = 0;
    qint64 dayOffset = 0;
    QString arrival;
    QString departure;
    if (!readString(object, "stationCode", &stop->stationCode, error)
        || !readInteger(object, "sequence", &sequence, error)
        || !readString(object, "arrivalTime", &arrival, error)
        || !readString(object, "departureTime", &departure, error)
        || !readInteger(object, "dayOffset", &dayOffset, error)) {
        return false;
    }
    stop->sequence = static_cast<int>(sequence);
    stop->dayOffset = static_cast<int>(dayOffset);
    stop->arrivalTime = arrival.isEmpty() ? QTime() : QTime::fromString(arrival, Qt::ISODate);
    stop->departureTime = departure.isEmpty() ? QTime() : QTime::fromString(departure, Qt::ISODate);
    if ((!arrival.isEmpty() && !stop->arrivalTime.isValid())
        || (!departure.isEmpty() && !stop->departureTime.isValid())) {
        *error = QObject::tr("经停站时间格式错误。");
        return false;
    }
    return true;
}

bool parseSegment(const QJsonValue &value, SegmentInventory *segment, QString *error)
{
    if (!value.isObject()) {
        *error = QObject::tr("区间票额记录格式错误。");
        return false;
    }
    qint64 totalSeats = 0;
    qint64 remainingSeats = 0;
    const QJsonObject object = value.toObject();
    if (!readInteger(object, "priceCents", &segment->priceCents, error)
        || !readInteger(object, "totalSeats", &totalSeats, error)
        || !readInteger(object, "remainingSeats", &remainingSeats, error)) {
        return false;
    }
    segment->totalSeats = static_cast<int>(totalSeats);
    segment->remainingSeats = static_cast<int>(remainingSeats);
    if (segment->priceCents < 0 || segment->totalSeats < 0 || segment->remainingSeats < 0
        || segment->remainingSeats > segment->totalSeats) {
        *error = QObject::tr("区间票价或票额超出有效范围。");
        return false;
    }
    return true;
}

bool parseSeat(const QJsonValue &value, SeatInventory *seat, QString *error)
{
    if (!value.isObject()) {
        *error = QObject::tr("席别记录格式错误。");
        return false;
    }
    QJsonArray segments;
    const QJsonObject object = value.toObject();
    if (!readString(object, "seatType", &seat->seatType, error)
        || !readArray(object, "segments", &segments, error)) {
        return false;
    }
    for (const QJsonValue &segmentValue : segments) {
        SegmentInventory segment;
        if (!parseSegment(segmentValue, &segment, error))
            return false;
        seat->segments.append(segment);
    }
    const QJsonValue detailsValue = object.value(QStringLiteral("details"));
    if (!detailsValue.isUndefined()) {
        if (!detailsValue.isArray()) {
            *error = QObject::tr("字段 details 应为数组。");
            return false;
        }
        QSet<QString> seatIds;
        for (const QJsonValue &detailValue : detailsValue.toArray()) {
            if (!detailValue.isObject()) {
                *error = QObject::tr("座位明细记录格式错误。");
                return false;
            }
            const QJsonObject detailObject = detailValue.toObject();
            SeatDetail detail;
            qint64 occupiedMask = 0;
            if (!readString(detailObject, "seatId", &detail.seatId, error)
                || !readInteger(detailObject, "occupiedMask", &occupiedMask, error)
                || detail.seatId.trimmed().isEmpty() || occupiedMask < 0
                || seatIds.contains(detail.seatId.toCaseFolded())) {
                if (error->isEmpty())
                    *error = QObject::tr("座位编号或占用掩码格式错误。");
                return false;
            }
            detail.occupiedMask = static_cast<quint64>(occupiedMask);
            seatIds.insert(detail.seatId.toCaseFolded());
            seat->details.append(detail);
        }
    }
    return true;
}

bool parseTrain(const QJsonValue &value, Train *train, QString *error)
{
    if (!value.isObject()) {
        *error = QObject::tr("车次记录格式错误。");
        return false;
    }
    const QJsonObject object = value.toObject();
    QJsonArray stops;
    QJsonArray seats;
    if (!readString(object, "number", &train->number, error)
        || !readArray(object, "stops", &stops, error)
        || !readArray(object, "seats", &seats, error)) {
        return false;
    }
    train->railwayTrainId = object.value(QStringLiteral("railwayTrainId")).toString();
    const QString railwayDate = object.value(QStringLiteral("railwayServiceDate")).toString();
    if (!railwayDate.isEmpty()) {
        train->railwayServiceDate = QDate::fromString(railwayDate, Qt::ISODate);
        if (!train->railwayServiceDate.isValid()) {
            *error = QObject::tr("12306 车次运行日期格式错误。");
            return false;
        }
    }
    for (const QJsonValue &stopValue : stops) {
        TrainStop stop;
        if (!parseStop(stopValue, &stop, error))
            return false;
        train->stops.append(stop);
    }
    for (const QJsonValue &seatValue : seats) {
        SeatInventory seat;
        if (!parseSeat(seatValue, &seat, error))
            return false;
        train->seats.append(seat);
    }
    if (train->stops.size() < 2) {
        *error = QObject::tr("车次至少需要两个经停站。");
        return false;
    }
    const int expectedSegments = train->stops.size() - 1;
    for (const SeatInventory &seat : train->seats) {
        if (seat.segments.size() != expectedSegments) {
            *error = QObject::tr("车次 %1 的席别区间数量与经停站不匹配。")
                         .arg(train->number);
            return false;
        }
    }
    if (expectedSegments > 53) {
        *error = QObject::tr("车次 %1 经停区间过多，座位掩码最多支持 53 个区间。").arg(train->number);
        return false;
    }
    for (SeatInventory &seat : train->seats) {
        if (seat.details.isEmpty())
            rebuildSeatDetails(&seat);
        else
            syncRemainingSeats(&seat);
    }
    return true;
}

bool parsePassenger(const QJsonValue &value, Passenger *passenger, QString *error)
{
    if (!value.isObject()) {
        *error = QObject::tr("乘车人记录格式错误。");
        return false;
    }
    const QJsonObject object = value.toObject();
    if (!readString(object, "ownerUserId", &passenger->ownerUserId, error))
        return false;
    return readString(object, "id", &passenger->id, error)
        && readString(object, "name", &passenger->name, error)
        && readString(object, "documentType", &passenger->documentType, error)
        && readString(object, "documentNumber", &passenger->documentNumber, error);
}

bool parseOrder(const QJsonValue &value, Order *order, QString *error)
{
    if (!value.isObject()) {
        *error = QObject::tr("订单记录格式错误。");
        return false;
    }
    const QJsonObject object = value.toObject();
    if (!readString(object, "ownerUserId", &order->ownerUserId, error))
        return false;
    QString createdAt;
    QJsonArray ticketIds;
    if (!readString(object, "id", &order->id, error)
        || !readString(object, "createdAt", &createdAt, error)
        || !readArray(object, "ticketIds", &ticketIds, error)
        || !readInteger(object, "totalAmountCents", &order->totalAmountCents, error)) {
        return false;
    }
    order->createdAt = QDateTime::fromString(createdAt, Qt::ISODate);
    if (!order->createdAt.isValid() || order->totalAmountCents < 0) {
        *error = QObject::tr("订单时间或金额格式错误。");
        return false;
    }
    for (const QJsonValue &ticketId : ticketIds) {
        if (!ticketId.isString()) {
            *error = QObject::tr("订单车票编号格式错误。");
            return false;
        }
        order->ticketIds.append(ticketId.toString());
    }
    return true;
}

bool parseTicket(const QJsonValue &value, Ticket *ticket, QString *error)
{
    if (!value.isObject()) {
        *error = QObject::tr("车票记录格式错误。");
        return false;
    }
    const QJsonObject object = value.toObject();
    QString status;
    if (!readString(object, "id", &ticket->id, error)
        || !readString(object, "passengerId", &ticket->passengerId, error)
        || !readString(object, "trainNumber", &ticket->trainNumber, error)
        || !readString(object, "fromStationCode", &ticket->fromStationCode, error)
        || !readString(object, "toStationCode", &ticket->toStationCode, error)
        || !readString(object, "seatType", &ticket->seatType, error)
        || !readInteger(object, "priceCents", &ticket->priceCents, error)
        || !readString(object, "status", &status, error)) {
        return false;
    }
    if (ticket->priceCents < 0 || !ticketStatusFromKey(status, &ticket->status)) {
        *error = QObject::tr("车票金额或状态格式错误。");
        return false;
    }
    // 第一至第四阶段生成的旧数据没有 serviceDate；保留兼容性，业务层会按唯一车次回退查找。
    const QJsonValue serviceDate = object.value(QStringLiteral("serviceDate"));
    if (!serviceDate.isUndefined()) {
        if (!serviceDate.isString()) {
            *error = QObject::tr("字段 serviceDate 应为文本。");
            return false;
        }
        ticket->serviceDate = QDate::fromString(serviceDate.toString(), Qt::ISODate);
        if (!ticket->serviceDate.isValid()) {
            *error = QObject::tr("车票运行日期格式错误。");
            return false;
        }
    }
    ticket->railwayTrainId = object.value(QStringLiteral("railwayTrainId")).toString();
    ticket->seatId = object.value(QStringLiteral("seatId")).toString();
    return true;
}

bool parseRefund(const QJsonValue &value, RefundRecord *refund, QString *error)
{
    if (!value.isObject()) {
        *error = QObject::tr("退票记录格式错误。");
        return false;
    }
    const QJsonObject object = value.toObject();
    QString processedAt;
    qint64 ratePercent = 0;
    if (!readString(object, "id", &refund->id, error)
        || !readString(object, "ticketId", &refund->ticketId, error)
        || !readString(object, "processedAt", &processedAt, error)
        || !readInteger(object, "ratePercent", &ratePercent, error)
        || !readInteger(object, "feeCents", &refund->feeCents, error)
        || !readInteger(object, "refundAmountCents", &refund->refundAmountCents, error)) {
        return false;
    }
    refund->processedAt = QDateTime::fromString(processedAt, Qt::ISODate);
    refund->ratePercent = static_cast<int>(ratePercent);
    if (!refund->processedAt.isValid() || refund->ratePercent < 0 || refund->ratePercent > 100
        || refund->feeCents < 0 || refund->refundAmountCents < 0) {
        *error = QObject::tr("退票记录金额、费率或时间格式错误。");
        return false;
    }
    return true;
}

template<typename T, typename Parser>
bool parseList(const QJsonArray &array, QVector<T> *output, Parser parser, QString *error)
{
    for (const QJsonValue &value : array) {
        T item;
        if (!parser(value, &item, error))
            return false;
        output->append(item);
    }
    return true;
}

bool appDataFromJson(const QJsonObject &object, AppData *data, QString *error)
{
    qint64 schemaVersion = 0;
    if (!readInteger(object, "schemaVersion", &schemaVersion, error))
        return false;
    if (schemaVersion != 4 && schemaVersion != CurrentSchemaVersion) {
        *error = QObject::tr("不支持的数据版本：%1，当前版本为 %2。")
                     .arg(schemaVersion)
                     .arg(CurrentSchemaVersion);
        return false;
    }
    data->schemaVersion = CurrentSchemaVersion;
    QJsonArray users;
    if (!readArray(object, "users", &users, error))
        return false;
    data->users.clear();
    for (const auto &value : users) {
        const QJsonObject entry = value.toObject();
        User user;
        if (!readString(entry, "id", &user.id, error)
            || !readString(entry, "username", &user.username, error)
            || !readString(entry, "password", &user.password, error)
            || !readString(entry, "role", &user.role, error))
            return false;
        user.username = user.username.trimmed();
        if (user.id.isEmpty() || user.username.isEmpty() || user.password.isEmpty()
            || (user.role != QStringLiteral("admin") && user.role != QStringLiteral("user"))) {
            *error = QObject::tr("账号记录格式错误。");
            return false;
        }
        for (const auto &existing : data->users) {
            if (existing.id == user.id
                || existing.username.compare(user.username, Qt::CaseInsensitive) == 0) {
                *error = QObject::tr("账号或账号 ID 重复。");
                return false;
            }
        }
        data->users.append(user);
    }

    QJsonArray stations;
    QJsonArray trains;
    QJsonArray passengers;
    QJsonArray orders;
    QJsonArray tickets;
    QJsonArray refunds;
    if (!readArray(object, "stations", &stations, error)
        || !readArray(object, "trains", &trains, error)
        || !readArray(object, "passengers", &passengers, error)
        || !readArray(object, "orders", &orders, error)
        || !readArray(object, "tickets", &tickets, error)
        || !readArray(object, "refunds", &refunds, error)) {
        return false;
    }

    const QJsonValue hiddenValue = object.value(QStringLiteral("hiddenTrainNumbers"));
    if (!hiddenValue.isUndefined()) {
        if (!hiddenValue.isArray()) {
            *error = QObject::tr("字段 hiddenTrainNumbers 应为数组。");
            return false;
        }
        for (const QJsonValue &value : hiddenValue.toArray()) {
            if (!value.isString()) {
                *error = QObject::tr("隐藏车次编号格式错误。");
                return false;
            }
            const QString number = value.toString().trimmed().toUpper();
            if (!number.isEmpty() && !data->hiddenTrainNumbers.contains(number))
                data->hiddenTrainNumbers.append(number);
        }
    }
    const bool parsed = parseList(stations, &data->stations, parseStation, error)
        && parseList(trains, &data->trains, parseTrain, error)
        && parseList(passengers, &data->passengers, parsePassenger, error)
        && parseList(orders, &data->orders, parseOrder, error)
        && parseList(tickets, &data->tickets, parseTicket, error)
        && parseList(refunds, &data->refunds, parseRefund, error);
    if (!parsed)
        return false;

    // 第 5 版早期数据按每车 20 排编号。按当前每车 14 排规则重排时，
    // 同步迁移票面 seatId，保持退票仍能定位到同一个座位索引。
    for (Train &train : data->trains) {
        for (SeatInventory &seat : train.seats) {
            QHash<QString, QString> renamedIds;
            for (int index = 0; index < seat.details.size(); ++index) {
                const QString oldId = seat.details.at(index).seatId;
                renamedIds.insert(oldId, seatIdForIndex(seat.seatType, index));
            }
            renumberSeatDetails(&seat);
            for (Ticket &ticket : data->tickets) {
                const bool trainMatches = ticket.railwayTrainId.isEmpty()
                    ? train.railwayTrainId.isEmpty() && train.number == ticket.trainNumber
                        && (!ticket.serviceDate.isValid()
                            || !train.railwayServiceDate.isValid()
                            || train.railwayServiceDate == ticket.serviceDate)
                    : train.railwayTrainId == ticket.railwayTrainId
                        && train.railwayServiceDate == ticket.serviceDate;
                if (trainMatches && ticket.seatType == seat.seatType
                    && renamedIds.contains(ticket.seatId)) {
                    ticket.seatId = renamedIds.value(ticket.seatId);
                }
            }
        }
    }

    QVector<Train> uniqueTrains;
    for (const Train &train : std::as_const(data->trains)) {
        const auto existing = std::find_if(uniqueTrains.begin(), uniqueTrains.end(), [&train](const Train &item) {
            if (!train.railwayTrainId.isEmpty() || !item.railwayTrainId.isEmpty()
                || train.railwayServiceDate.isValid() || item.railwayServiceDate.isValid())
                return item.railwayTrainId == train.railwayTrainId
                    && item.railwayServiceDate == train.railwayServiceDate
                    && item.number.compare(train.number, Qt::CaseInsensitive) == 0;
            return item.number.compare(train.number, Qt::CaseInsensitive) == 0;
        });
        if (existing == uniqueTrains.end())
            uniqueTrains.append(train);
        else
            *existing = train;
    }
    data->trains = std::move(uniqueTrains);
    return true;
}
} // 命名空间

JsonRepository::JsonRepository(QString filePath)
    : m_filePath(QDir::cleanPath(std::move(filePath)))
{
}

LoadResult JsonRepository::load() const
{
    if (!QFileInfo::exists(m_filePath))
        return {false, true, {}, {}};

    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly))
        return {false, false, {}, QObject::tr("无法读取数据文件：%1").arg(file.errorString())};

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return {false,
                false,
                {},
                QObject::tr("数据文件不是有效的 JSON：%1").arg(parseError.errorString())};
    }

    domain::AppData data;
    QString error;
    if (!appDataFromJson(document.object(), &data, &error))
        return {false, false, {}, error};

    return {true, false, data, {}};
}

OperationResult JsonRepository::save(const domain::AppData &data) const
{
    const QFileInfo fileInfo(m_filePath);
    QDir directory(fileInfo.absolutePath());
    if (!directory.exists() && !directory.mkpath(QStringLiteral(".")))
        return OperationResult::failure(
            QObject::tr("无法创建数据目录：%1").arg(directory.absolutePath()));

    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly))
        return OperationResult::failure(
            QObject::tr("无法写入数据文件：%1").arg(file.errorString()));

    const QByteArray json = QJsonDocument(appDataToJson(data)).toJson(QJsonDocument::Indented);
    if (file.write(json) != json.size()) {
        file.cancelWriting();
        return OperationResult::failure(QObject::tr("写入数据失败：%1").arg(file.errorString()));
    }
    if (!file.commit())
        return OperationResult::failure(
            QObject::tr("提交数据文件失败：%1").arg(file.errorString()));

    return OperationResult::ok();
}

QString JsonRepository::dataFilePath() const
{
    return m_filePath;
}

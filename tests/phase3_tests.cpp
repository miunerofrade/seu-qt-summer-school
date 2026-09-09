#include "data/datastore.h"
#include "data/jsonrepository.h"
#include "features/query/trainqueryfilterproxymodel.h"
#include "features/query/trainquerymodel.h"
#include "features/query/railwayqueryservice.h"
#include "features/query/queryservice.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHash>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>
#include <memory>

class PhaseThreeTests final : public QObject
{
    Q_OBJECT

private slots:
    void directQueryAggregatesPriceAndInventory();
    void multiSegmentQueryUsesRequestedRange();
    void fourStationLongQueryUsesMinimumCoveredInventory();
    void disabledAndInvalidRoutesAreExcluded();
    void proxyFiltersAndSortsRows();
    void availableQueryExcludesDepartedTrains();
    void parsesRailwayTicketResponse();
    void parsesConventionalTrainSeatPrices();
    void parsesFullStationCatalogFormat();
    void parsesRailwayRouteAndCrossDay();
    void hiddenTrainsAreRemovedFromModel();
    void recurringCustomRouteHandlesCrossDay();
};

namespace {
std::unique_ptr<DataStore> initializedStore(const QString &path)
{
    auto store = std::make_unique<DataStore>(std::make_unique<JsonRepository>(path));
    if (!store->initialize())
        return {};
    return store;
}
}

void PhaseThreeTests::directQueryAggregatesPriceAndInventory()
{
    QTemporaryDir directory;
    auto store = initializedStore(directory.filePath(QStringLiteral("app.json")));
    QVERIFY(store);

    QueryService service(store.get());
    const auto rows = service.query({QStringLiteral("NKH"), QStringLiteral("AOH"), QDate::currentDate()});
    QCOMPARE(rows.size(), 2);

    const auto it = std::find_if(rows.cbegin(), rows.cend(), [](const TrainQueryRow &row) {
        return row.trainNumber == QStringLiteral("G101");
    });
    QVERIFY(it != rows.cend());
    QCOMPARE(it->seats.size(), 2);
    const auto seat = std::find_if(it->seats.cbegin(), it->seats.cend(), [](const TrainSeatOption &option) {
        return option.seatType == QStringLiteral("二等座");
    });
    QVERIFY(seat != it->seats.cend());
    QCOMPARE(seat->priceCents, qint64(15000));
    QCOMPARE(seat->remainingSeats, 35);
    QCOMPARE(it->departureTime, QTime(8, 0));
    QCOMPARE(it->arrivalTime, QTime(9, 20));
    QCOMPARE(it->durationMinutes, 80);
}

void PhaseThreeTests::multiSegmentQueryUsesRequestedRange()
{
    QTemporaryDir directory;
    auto store = initializedStore(directory.filePath(QStringLiteral("app.json")));
    QVERIFY(store);

    QueryService service(store.get());
    const auto rows = service.query({QStringLiteral("NKH"), QStringLiteral("HGH"), QDate::currentDate()});
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().trainNumber, QStringLiteral("G205"));
    QCOMPARE(rows.first().seats.size(), 2);
    QCOMPARE(rows.first().seats.at(0).seatType, QStringLiteral("一等座"));
    QCOMPARE(rows.first().seats.at(0).priceCents, qint64(35400));
    QCOMPARE(rows.first().seats.at(0).remainingSeats, 79);
    QCOMPARE(rows.first().seats.at(1).seatType, QStringLiteral("二等座"));
    QCOMPARE(rows.first().seats.at(1).priceCents, qint64(22200));
    QCOMPARE(rows.first().seats.at(1).remainingSeats, 58);
}

void PhaseThreeTests::fourStationLongQueryUsesMinimumCoveredInventory()
{
    QTemporaryDir directory;
    auto store = initializedStore(directory.filePath(QStringLiteral("app.json")));
    QVERIFY(store);
    domain::AppData candidate = store->data();
    candidate.stations.append({QStringLiteral("S1"), QStringLiteral("一站"), QStringLiteral("一站"), true});
    candidate.stations.append({QStringLiteral("S2"), QStringLiteral("二站"), QStringLiteral("二站"), true});
    candidate.stations.append({QStringLiteral("S3"), QStringLiteral("三站"), QStringLiteral("三站"), true});
    candidate.stations.append({QStringLiteral("S4"), QStringLiteral("四站"), QStringLiteral("四站"), true});
    domain::Train train;
    train.number = QStringLiteral("T4");
    train.stops = {{QStringLiteral("S1"), 0, {}, QTime(8, 0)},
                   {QStringLiteral("S2"), 1, QTime(8, 20), QTime(8, 22)},
                   {QStringLiteral("S3"), 2, QTime(8, 40), QTime(8, 42)},
                   {QStringLiteral("S4"), 3, QTime(9, 0), {}}};
    train.seats = {{QStringLiteral("二等座"), {{100, 10}, {100, 10}, {100, 10}}}};
    candidate.trains.append(train);
    QVERIFY(store->commit(candidate));

    const auto rows = QueryService(store.get()).query(
        {QStringLiteral("S1"), QStringLiteral("S4"), QDate::currentDate()});
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().seats.size(), 1);
    QCOMPARE(rows.first().seats.first().remainingSeats, 10);
    QCOMPARE(rows.first().seats.first().priceCents, qint64(300));
}

void PhaseThreeTests::disabledAndInvalidRoutesAreExcluded()
{
    QTemporaryDir directory;
    auto store = initializedStore(directory.filePath(QStringLiteral("app.json")));
    QVERIFY(store);
    QueryService service(store.get());

    QVERIFY(service.query({QStringLiteral("NKH"), QStringLiteral("NKH"), QDate::currentDate()}).isEmpty());
    QVERIFY(service.query({QStringLiteral("AOH"), QStringLiteral("NKH"), QDate::currentDate()}).isEmpty());

    auto candidate = store->data();
    candidate.hiddenTrainNumbers.append(QStringLiteral("G101"));
    QVERIFY(store->commit(candidate));
    const auto rows = service.query({QStringLiteral("NKH"), QStringLiteral("AOH"), QDate::currentDate()});
    QCOMPARE(rows.size(), 1);
    for (const TrainQueryRow &row : rows)
        QCOMPARE(row.trainNumber, QStringLiteral("G205"));
}

void PhaseThreeTests::proxyFiltersAndSortsRows()
{
    QTemporaryDir directory;
    auto store = initializedStore(directory.filePath(QStringLiteral("app.json")));
    QVERIFY(store);
    QueryService service(store.get());
    TrainQueryModel model;
    model.setRows(service.query({QStringLiteral("NKH"), QStringLiteral("AOH"), QDate::currentDate()}));
    TrainQueryFilterProxyModel proxy;
    proxy.setSourceModel(&model);

    QCOMPARE(proxy.rowCount(), 2);
    proxy.setTrainNumberFilter(QStringLiteral("G101"));
    QCOMPARE(proxy.rowCount(), 1);
    proxy.setSeatTypeFilter(QStringLiteral("一等座"));
    QCOMPARE(proxy.rowCount(), 1);
    proxy.setTrainNumberFilter({});
    proxy.setSeatTypeFilter({});
    proxy.setAvailableOnly(true);
    model.selectSeatType({}, true);
    QCOMPARE(proxy.rowCount(), 2);
    proxy.setSortByPrice(true);
    QCOMPARE(proxy.index(0, TrainQueryModel::PriceColumn).data().toString(), QStringLiteral("¥149.00"));

    const QModelIndex g205Seat = model.index(1, TrainQueryModel::SeatTypeColumn);
    QVERIFY(model.setData(g205Seat, QStringLiteral("一等座")));
    QCOMPARE(model.index(1, TrainQueryModel::PriceColumn).data().toString(), QStringLiteral("¥238.00"));
}

void PhaseThreeTests::availableQueryExcludesDepartedTrains()
{
    QTemporaryDir directory;
    auto store = initializedStore(directory.filePath(QStringLiteral("app.json")));
    QVERIFY(store);

    const QDate date(2030, 1, 10);

    QueryService service(store.get());
    const auto rows = service.available(QDateTime(date, QTime(10, 0)));
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().trainNumber, QStringLiteral("G205"));
    QCOMPARE(rows.first().serviceDate, date);
    QCOMPARE(rows.first().departureStationCode, QStringLiteral("NKH"));
    QCOMPARE(rows.first().arrivalStationCode, QStringLiteral("HGH"));

    const auto queried = service.query(
        {QStringLiteral("NKH"), QStringLiteral("AOH"), date},
        QDateTime(date, QTime(10, 0)));
    QCOMPARE(queried.size(), 1);
    QCOMPARE(queried.first().trainNumber, QStringLiteral("G205"));
}

void PhaseThreeTests::parsesRailwayTicketResponse()
{
    QStringList fields;
    fields.fill({}, 58);
    fields[2] = QStringLiteral("65000G09850A");
    fields[3] = QStringLiteral("G985");
    fields[4] = QStringLiteral("QDK");
    fields[5] = QStringLiteral("AOH");
    fields[6] = QStringLiteral("NKH");
    fields[7] = QStringLiteral("AOH");
    fields[8] = QStringLiteral("15:02");
    fields[9] = QStringLiteral("16:10");
    fields[10] = QStringLiteral("01:08");
    fields[11] = QStringLiteral("Y");
    fields[30] = QStringLiteral("有");
    fields[31] = QStringLiteral("12");
    fields[29] = QStringLiteral("有");
    fields[26] = QStringLiteral("有");
    fields[39] = QStringLiteral("M026000021O0162000211005800021");
    const QJsonObject root{
        {QStringLiteral("status"), true},
        {QStringLiteral("data"),
         QJsonObject{{QStringLiteral("map"),
                      QJsonObject{{QStringLiteral("NKH"), QStringLiteral("南京南")},
                                  {QStringLiteral("AOH"), QStringLiteral("上海虹桥")}}},
                     {QStringLiteral("result"), QJsonArray{fields.join(QLatin1Char('|'))}}}}
    };

    QVector<TrainQueryRow> rows;
    QString error;
    const QDate tomorrow = QDate::currentDate().addDays(1);
    QVERIFY(RailwayQueryService::parseTicketResponse(
        QJsonDocument(root).toJson(QJsonDocument::Compact), tomorrow, &rows, &error));
    QCOMPARE(error, QString());
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().trainNumber, QStringLiteral("G985"));
    QCOMPARE(rows.first().railwayTrainId, QStringLiteral("65000G09850A"));
    QCOMPARE(rows.first().originStationName, QStringLiteral("QDK"));
    QCOMPARE(rows.first().terminalStationName, QStringLiteral("AOH"));
    QCOMPARE(rows.first().departureStationName, QStringLiteral("南京南"));
    QCOMPARE(rows.first().arrivalStationName, QStringLiteral("上海虹桥"));
    QCOMPARE(rows.first().durationMinutes, 68);
    QCOMPARE(rows.first().seats.size(), 4);
    QCOMPARE(rows.first().seats.first().seatType, QStringLiteral("一等座"));
    QCOMPARE(rows.first().seats.first().availabilityText, QStringLiteral("12"));
    QCOMPARE(rows.first().seats.first().priceCents, qint64(26000));
    QCOMPARE(rows.first().seats.at(1).priceCents, qint64(16200));
    QCOMPARE(rows.first().seats.at(1).remainingSeats, 50);
    const auto hardSeat = std::find_if(rows.first().seats.cbegin(), rows.first().seats.cend(), [](const TrainSeatOption &seat) {
        return seat.seatType == QStringLiteral("硬座");
    });
    const auto noSeat = std::find_if(rows.first().seats.cbegin(), rows.first().seats.cend(), [](const TrainSeatOption &seat) {
        return seat.seatType == QStringLiteral("无座");
    });
    QVERIFY(hardSeat != rows.first().seats.cend());
    QVERIFY(noSeat != rows.first().seats.cend());
    QCOMPARE(hardSeat->priceCents, qint64(5800));
    QCOMPARE(noSeat->priceCents, hardSeat->priceCents);
    QVERIFY(!rows.first().bookable);

    TrainQueryModel model;
    model.setRows(rows);
    model.selectSeatType({}, false);
    QCOMPARE(model.selectedSeatAt(0)->seatType, QStringLiteral("二等座"));
}

void PhaseThreeTests::parsesConventionalTrainSeatPrices()
{
    QStringList fields;
    fields.fill({}, 58);
    fields[2] = QStringLiteral("55000K15050A");
    fields[3] = QStringLiteral("K1505");
    fields[4] = QStringLiteral("NJH");
    fields[5] = QStringLiteral("SHH");
    fields[6] = QStringLiteral("NJH");
    fields[7] = QStringLiteral("SHH");
    fields[8] = QStringLiteral("08:00");
    fields[9] = QStringLiteral("12:00");
    fields[10] = QStringLiteral("04:00");
    for (const int index : {23, 24, 26, 28, 29})
        fields[index] = QStringLiteral("有");
    fields[39] = QStringLiteral("4014050021300925002120072500211004650021");
    const QJsonObject root{{QStringLiteral("status"), true},
                           {QStringLiteral("data"),
                            QJsonObject{{QStringLiteral("map"),
                                         QJsonObject{{QStringLiteral("NJH"), QStringLiteral("南京")},
                                                     {QStringLiteral("SHH"), QStringLiteral("上海")}}},
                                        {QStringLiteral("result"), QJsonArray{fields.join(QLatin1Char('|'))}}}}};
    QVector<TrainQueryRow> rows;
    QString error;
    QVERIFY(RailwayQueryService::parseTicketResponse(
        QJsonDocument(root).toJson(QJsonDocument::Compact), QDate::currentDate().addDays(1), &rows, &error));
    QCOMPARE(rows.size(), 1);
    const QHash<QString, qint64> expected{{QStringLiteral("软卧"), 14050},
                                         {QStringLiteral("硬卧"), 9250},
                                         {QStringLiteral("软座"), 7250},
                                         {QStringLiteral("硬座"), 4650},
                                         {QStringLiteral("无座"), 4650}};
    for (auto it = expected.cbegin(); it != expected.cend(); ++it) {
        const auto seat = std::find_if(rows.first().seats.cbegin(), rows.first().seats.cend(), [&it](const TrainSeatOption &item) {
            return item.seatType == it.key();
        });
        QVERIFY(seat != rows.first().seats.cend());
        QCOMPARE(seat->priceCents, it.value());
    }
}

void PhaseThreeTests::parsesRailwayRouteAndCrossDay()
{
    const QJsonArray route{
        QJsonObject{{QStringLiteral("station_name"), QStringLiteral("甲站")},
                    {QStringLiteral("arrive_time"), QStringLiteral("----")},
                    {QStringLiteral("start_time"), QStringLiteral("23:30")}},
        QJsonObject{{QStringLiteral("station_name"), QStringLiteral("乙站")},
                    {QStringLiteral("arrive_time"), QStringLiteral("00:20")},
                    {QStringLiteral("start_time"), QStringLiteral("00:25")}},
        QJsonObject{{QStringLiteral("station_name"), QStringLiteral("丙站")},
                    {QStringLiteral("arrive_time"), QStringLiteral("01:10")},
                    {QStringLiteral("start_time"), QStringLiteral("01:10")}}};
    const QJsonObject root{{QStringLiteral("status"), true},
                           {QStringLiteral("data"), QJsonObject{{QStringLiteral("data"), route}}}};
    QVector<RailwayRouteStop> stops;
    QString error;
    QVERIFY(RailwayQueryService::parseRouteResponse(
        QJsonDocument(root).toJson(QJsonDocument::Compact),
        {{QStringLiteral("甲站"), QStringLiteral("AAA")},
         {QStringLiteral("乙站"), QStringLiteral("BBB")},
         {QStringLiteral("丙站"), QStringLiteral("CCC")}},
        &stops,
        &error));
    QCOMPARE(stops.size(), 3);
    QCOMPARE(stops.at(1).code, QStringLiteral("BBB"));
    QCOMPARE(stops.at(0).dayOffset, 0);
    QCOMPARE(stops.at(1).dayOffset, 1);
    QCOMPARE(stops.at(2).dayOffset, 1);
}

void PhaseThreeTests::parsesFullStationCatalogFormat()
{
    const QByteArray payload =
        "var station_names ='@njn|南京南|NKH|nanjingnan|njn|1|0705|南京|||"
        "@shq|上海虹桥|AOH|shanghaihongqiao|shhq|2|0712|上海|||';";
    const auto stations = RailwayQueryService::parseStationCatalog(payload);
    QCOMPARE(stations.size(), 2);
    const auto nanjing = std::find_if(stations.cbegin(), stations.cend(), [](const RailwayStation &station) {
        return station.name == QStringLiteral("南京南");
    });
    const auto shanghai = std::find_if(stations.cbegin(), stations.cend(), [](const RailwayStation &station) {
        return station.name == QStringLiteral("上海虹桥");
    });
    QVERIFY(nanjing != stations.cend());
    QVERIFY(shanghai != stations.cend());
    QCOMPARE(nanjing->code, QStringLiteral("NKH"));
    QCOMPARE(shanghai->code, QStringLiteral("AOH"));
}

void PhaseThreeTests::hiddenTrainsAreRemovedFromModel()
{
    TrainQueryRow visible;
    visible.trainNumber = QStringLiteral("G101");
    visible.serviceDate = QDate::currentDate();
    visible.seats.append({QStringLiteral("二等座"), 10, 1000});
    TrainQueryRow hidden = visible;
    hidden.trainNumber = QStringLiteral("G205");

    TrainQueryModel model;
    model.setHiddenTrainNumbers({QStringLiteral("G205")});
    model.setRows({visible, hidden});
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.index(0, TrainQueryModel::TrainNumberColumn).data().toString(), QStringLiteral("G101"));
}

void PhaseThreeTests::recurringCustomRouteHandlesCrossDay()
{
    QTemporaryDir directory;
    auto store = initializedStore(directory.filePath(QStringLiteral("app.json")));
    QVERIFY(store);
    domain::AppData candidate = store->data();
    candidate.stations.append({QStringLiteral("CUS"), QStringLiteral("自定义站"), QStringLiteral("自定义城市"), true});
    domain::Train train;
    train.number = QStringLiteral("L900");
    train.stops = {{QStringLiteral("CUS"), 0, {}, QTime(23, 30), 0},
                   {QStringLiteral("AOH"), 1, QTime(0, 30), {}, 1}};
    train.seats = {{QStringLiteral("硬座"), {{5000, 20}}}};
    candidate.trains.append(train);
    QVERIFY(store->commit(candidate));

    const QDate date(2030, 1, 10);
    const auto rows = QueryService(store.get()).query(
        {QStringLiteral("CUS"), QStringLiteral("AOH"), date},
        QDateTime(date, QTime(20, 0)));
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().serviceDate, date);
    QCOMPARE(rows.first().departureDayOffset, 0);
    QCOMPARE(rows.first().arrivalDayOffset, 1);
    QCOMPARE(rows.first().durationMinutes, 60);
}

QTEST_GUILESS_MAIN(PhaseThreeTests)
#include "phase3_tests.moc"

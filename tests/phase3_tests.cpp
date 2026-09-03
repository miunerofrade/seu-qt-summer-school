#include "data/datastore.h"
#include "data/jsonrepository.h"
#include "models/trainqueryfilterproxymodel.h"
#include "models/trainquerymodel.h"
#include "services/railwayqueryservice.h"
#include "services/queryservice.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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
    void disabledAndInvalidRoutesAreExcluded();
    void proxyFiltersAndSortsRows();
    void availableQueryExcludesDepartedTrains();
    void parsesRailwayTicketResponse();
    void parsesFullStationCatalogFormat();
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
    const auto rows = service.query({QStringLiteral("NJN"), QStringLiteral("SHH"), QDate::currentDate()});
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
    const auto rows = service.query({QStringLiteral("NJN"), QStringLiteral("HGH"), QDate::currentDate()});
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().trainNumber, QStringLiteral("G205"));
    QCOMPARE(rows.first().seats.size(), 2);
    QCOMPARE(rows.first().seats.at(0).priceCents, qint64(22200));
    QCOMPARE(rows.first().seats.at(0).remainingSeats, 58);
    QCOMPARE(rows.first().seats.at(1).priceCents, qint64(35400));
    QCOMPARE(rows.first().seats.at(1).remainingSeats, 15);
}

void PhaseThreeTests::disabledAndInvalidRoutesAreExcluded()
{
    QTemporaryDir directory;
    auto store = initializedStore(directory.filePath(QStringLiteral("app.json")));
    QVERIFY(store);
    QueryService service(store.get());

    QVERIFY(service.query({QStringLiteral("NJN"), QStringLiteral("NJN"), QDate::currentDate()}).isEmpty());
    QVERIFY(service.query({QStringLiteral("SHH"), QStringLiteral("NJN"), QDate::currentDate()}).isEmpty());

    auto candidate = store->data();
    candidate.hiddenTrainNumbers.append(QStringLiteral("G101"));
    QVERIFY(store->commit(candidate));
    const auto rows = service.query({QStringLiteral("NJN"), QStringLiteral("SHH"), QDate::currentDate()});
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
    model.setRows(service.query({QStringLiteral("NJN"), QStringLiteral("SHH"), QDate::currentDate()}));
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
    QCOMPARE(rows.first().departureStationCode, QStringLiteral("NJN"));
    QCOMPARE(rows.first().arrivalStationCode, QStringLiteral("HGH"));

    const auto queried = service.query(
        {QStringLiteral("NJN"), QStringLiteral("SHH"), date},
        QDateTime(date, QTime(10, 0)));
    QCOMPARE(queried.size(), 1);
    QCOMPARE(queried.first().trainNumber, QStringLiteral("G205"));
}

void PhaseThreeTests::parsesRailwayTicketResponse()
{
    QStringList fields;
    fields.fill({}, 58);
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
    fields[39] = QStringLiteral("M026000021O016200021");
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
    QCOMPARE(rows.first().originStationName, QStringLiteral("QDK"));
    QCOMPARE(rows.first().terminalStationName, QStringLiteral("AOH"));
    QCOMPARE(rows.first().departureStationName, QStringLiteral("南京南"));
    QCOMPARE(rows.first().arrivalStationName, QStringLiteral("上海虹桥"));
    QCOMPARE(rows.first().durationMinutes, 68);
    QCOMPARE(rows.first().seats.size(), 2);
    QCOMPARE(rows.first().seats.first().seatType, QStringLiteral("一等座"));
    QCOMPARE(rows.first().seats.first().availabilityText, QStringLiteral("12"));
    QCOMPARE(rows.first().seats.first().priceCents, qint64(26000));
    QCOMPARE(rows.first().seats.at(1).priceCents, qint64(16200));
    QVERIFY(!rows.first().bookable);

    TrainQueryModel model;
    model.setRows(rows);
    model.selectSeatType({}, false);
    QCOMPARE(model.selectedSeatAt(0)->seatType, QStringLiteral("二等座"));
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
                   {QStringLiteral("SHH"), 1, QTime(0, 30), {}, 1}};
    train.seats = {{QStringLiteral("硬座"), {{5000, 20, 20}}}};
    candidate.trains.append(train);
    QVERIFY(store->commit(candidate));

    const QDate date(2030, 1, 10);
    const auto rows = QueryService(store.get()).query(
        {QStringLiteral("CUS"), QStringLiteral("SHH"), date},
        QDateTime(date, QTime(20, 0)));
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().serviceDate, date);
    QCOMPARE(rows.first().departureDayOffset, 0);
    QCOMPARE(rows.first().arrivalDayOffset, 1);
    QCOMPARE(rows.first().durationMinutes, 60);
}

QTEST_GUILESS_MAIN(PhaseThreeTests)
#include "phase3_tests.moc"

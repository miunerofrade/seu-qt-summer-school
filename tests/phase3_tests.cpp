#include "data/datastore.h"
#include "data/jsonrepository.h"
#include "models/trainqueryfilterproxymodel.h"
#include "models/trainquerymodel.h"
#include "services/queryservice.h"

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
    candidate.trains[0].enabled = false;
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

    auto candidate = store->data();
    const QDate date(2030, 1, 10);
    candidate.trains[0].serviceDate = date;
    candidate.trains[1].serviceDate = date.addDays(1);
    QVERIFY(store->commit(candidate));

    QueryService service(store.get());
    const auto rows = service.available(QDateTime(date, QTime(10, 0)));
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().trainNumber, QStringLiteral("G205"));
    QCOMPARE(rows.first().serviceDate, date.addDays(1));
    QCOMPARE(rows.first().departureStationCode, QStringLiteral("NJN"));
    QCOMPARE(rows.first().arrivalStationCode, QStringLiteral("HGH"));

    const auto queried = service.query(
        {QStringLiteral("NJN"), QStringLiteral("SHH"), date},
        QDateTime(date, QTime(10, 0)));
    QVERIFY(queried.isEmpty());
}

QTEST_GUILESS_MAIN(PhaseThreeTests)
#include "phase3_tests.moc"

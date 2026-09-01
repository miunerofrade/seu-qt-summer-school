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
    QCOMPARE(rows.size(), 4);

    const auto it = std::find_if(rows.cbegin(), rows.cend(), [](const TrainQueryRow &row) {
        return row.trainNumber == QStringLiteral("G101") && row.seatType == QStringLiteral("二等座");
    });
    QVERIFY(it != rows.cend());
    QCOMPARE(it->priceCents, qint64(15000));
    QCOMPARE(it->remainingSeats, 35);
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
    QCOMPARE(rows.size(), 2);
    for (const TrainQueryRow &row : rows) {
        QCOMPARE(row.trainNumber, QStringLiteral("G205"));
        QCOMPARE(row.priceCents, row.seatType == QStringLiteral("二等座") ? qint64(22200) : qint64(35400));
        QCOMPARE(row.remainingSeats, row.seatType == QStringLiteral("二等座") ? 58 : 15);
    }
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
    QCOMPARE(rows.size(), 2);
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

    QCOMPARE(proxy.rowCount(), 4);
    proxy.setTrainNumberFilter(QStringLiteral("G101"));
    QCOMPARE(proxy.rowCount(), 2);
    proxy.setSeatTypeFilter(QStringLiteral("一等座"));
    QCOMPARE(proxy.rowCount(), 1);
    proxy.setTrainNumberFilter({});
    proxy.setSeatTypeFilter({});
    proxy.setAvailableOnly(true);
    QCOMPARE(proxy.rowCount(), 4);
    proxy.setSortByPrice(true);
    QCOMPARE(proxy.index(0, TrainQueryModel::PriceColumn).data().toString(), QStringLiteral("¥149.00"));
}

QTEST_GUILESS_MAIN(PhaseThreeTests)
#include "phase3_tests.moc"

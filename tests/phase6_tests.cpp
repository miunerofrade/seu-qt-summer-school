#include "data/datastore.h"
#include "data/jsonrepository.h"
#include "services/statisticsservice.h"

#include <QTemporaryDir>
#include <QtTest>

#include <memory>

namespace {
std::unique_ptr<DataStore> storeFor(const QString &path)
{
    auto store = std::make_unique<DataStore>(std::make_unique<JsonRepository>(path));
    return store->initialize() ? std::move(store) : nullptr;
}
}

class PhaseSixTests final : public QObject
{
    Q_OBJECT

private slots:
    void aggregatesTicketsRefundsAndNetRevenue();
    void filtersByTrainSeatStationAndDate();
};

void PhaseSixTests::aggregatesTicketsRefundsAndNetRevenue()
{
    QTemporaryDir directory;
    auto store = storeFor(directory.filePath(QStringLiteral("app.json")));
    QVERIFY(store);
    const QDate date = QDate::currentDate();
    const QString passenger = store->data().passengers.first().id;
    domain::AppData data = store->data();
    data.orders.append({QStringLiteral("order-1"), QDateTime::currentDateTime(), {QStringLiteral("t1"), QStringLiteral("t2")}, 25000});
    data.tickets.append({QStringLiteral("t1"), passenger, QStringLiteral("G101"), QStringLiteral("NKH"), QStringLiteral("AOH"), QStringLiteral("二等座"), 15000, domain::TicketStatus::Issued, date});
    data.tickets.append({QStringLiteral("t2"), passenger, QStringLiteral("G101"), QStringLiteral("NKH"), QStringLiteral("AOH"), QStringLiteral("二等座"), 10000, domain::TicketStatus::Refunded, date});
    data.refunds.append({QStringLiteral("r1"), QStringLiteral("t2"), QDateTime::currentDateTime(), 0, 0, 10000});
    QVERIFY(store->commit(data));

    const StatisticsSummary summary = StatisticsService(store.get()).summarize({date, date, {}, {}, {}});
    QCOMPARE(summary.soldCount, 2);
    QCOMPARE(summary.refundedCount, 1);
    QCOMPARE(summary.netRevenueCents, qint64(15000));
    QVERIFY(!summary.rows.isEmpty());
    QVERIFY(summary.averageRemainingRate > 0.0);
    QCOMPARE(summary.dailyTrend.size(), 1);
    QCOMPARE(summary.dailyTrend.first().date, date);
    QCOMPARE(summary.dailyTrend.first().soldCount, 2);
    QCOMPARE(summary.dailyTrend.first().refundedCount, 1);
    QCOMPARE(summary.dailyTrend.first().netRevenueCents, qint64(15000));
    QCOMPARE(summary.seatShares.size(), 1);
    QCOMPARE(summary.seatShares.first().seatType, QStringLiteral("二等座"));
    QCOMPARE(summary.seatShares.first().soldCount, 2);
}

void PhaseSixTests::filtersByTrainSeatStationAndDate()
{
    QTemporaryDir directory;
    auto store = storeFor(directory.filePath(QStringLiteral("app.json")));
    QVERIFY(store);
    const QDate date = QDate::currentDate();
    const QString passenger = store->data().passengers.first().id;
    domain::AppData data = store->data();
    data.orders.append({QStringLiteral("order-2"), QDateTime::currentDateTime(), {QStringLiteral("t3")}, 25000});
    data.tickets.append({QStringLiteral("t3"), passenger, QStringLiteral("G101"), QStringLiteral("NKH"), QStringLiteral("AOH"), QStringLiteral("一等座"), 25000, domain::TicketStatus::Issued, date});
    QVERIFY(store->commit(data));

    StatisticsService service(store.get());
    QVERIFY(service.summarize({date, date, QStringLiteral("G101"), QStringLiteral("NKH"), QStringLiteral("一等座")}).soldCount == 1);
    QVERIFY(service.summarize({date.addDays(1), date.addDays(1), {}, {}, {}}).rows.isEmpty());
    QVERIFY(service.summarize({date, date, {}, {}, QStringLiteral("不存在")}).rows.isEmpty());
}

QTEST_GUILESS_MAIN(PhaseSixTests)

#include "phase6_tests.moc"

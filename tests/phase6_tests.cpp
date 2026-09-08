#include "data/datastore.h"
#include "data/jsonrepository.h"
#include "features/statistics/statisticsservice.h"

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

    const StatisticsSummary summary = StatisticsService(store.get()).summarize();
    QCOMPARE(summary.soldCount, 2);
    QCOMPARE(summary.refundedCount, 1);
    QCOMPARE(summary.netRevenueCents, qint64(15000));
    QVERIFY(summary.averageRemainingRate > 0.0);
}

QTEST_GUILESS_MAIN(PhaseSixTests)

#include "phase6_tests.moc"

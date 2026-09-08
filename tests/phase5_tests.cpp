#include "data/datastore.h"
#include "data/idatarepository.h"
#include "data/jsonrepository.h"
#include "features/booking/bookingservice.h"
#include "features/orders/orderservice.h"
#include "features/orders/refundservice.h"

#include <QTemporaryDir>
#include <QtTest>

#include <memory>

namespace {
std::unique_ptr<DataStore> initializedStore(const QString &path, const QDate &serviceDate)
{
    auto store = std::make_unique<DataStore>(std::make_unique<JsonRepository>(path));
    Q_UNUSED(serviceDate);
    if (!store->initialize() || !store->commit(domain::createDemoData())
        || !store->login(QStringLiteral("admin"), QStringLiteral("admin")))
        return {};
    return store;
}

QString bookOne(DataStore *store, const QDate &serviceDate)
{
    BookingReceipt receipt;
    const OperationResult result = BookingService(*store).book(
        {QStringLiteral("G101"), serviceDate, QStringLiteral("NKH"), QStringLiteral("AOH"),
         QStringLiteral("二等座"), {store->data().passengers.first().id}},
        &receipt);
    return result && !receipt.ticketIds.isEmpty() ? receipt.ticketIds.first() : QString();
}

class FailingRepository final : public IDataRepository
{
public:
    explicit FailingRepository(domain::AppData data)
        : m_data(std::move(data))
    {
    }

    LoadResult load() const override { return {true, false, m_data, {}}; }
    OperationResult save(const domain::AppData &) const override
    {
        return OperationResult::failure(QStringLiteral("模拟保存失败"));
    }
    QString dataFilePath() const override { return QStringLiteral("memory://failing"); }

private:
    domain::AppData m_data;
};
} // 命名空间

class PhaseFiveTests final : public QObject
{
    Q_OBJECT

private slots:
    void refundRateBoundariesAndRounding();
    void refundRestoresEverySegmentAndPersistsRecord();
    void departedAndRepeatedTicketsCannotBeRefunded();
    void completedTicketStatusRefreshesAtArrival();
    void saveFailureRollsBackRefundAndInventory();
};

void PhaseFiveTests::refundRateBoundariesAndRounding()
{
    QTemporaryDir directory;
    const QDate serviceDate = QDate::currentDate().addDays(12);
    auto store = initializedStore(directory.filePath(QStringLiteral("app.json")), serviceDate);
    QVERIFY(store);
    const QString ticketId = bookOne(store.get(), serviceDate);
    QVERIFY(!ticketId.isEmpty());
    const QDateTime departure(serviceDate, QTime(8, 0));
    RefundService service(store.get());
    RefundQuote quote;

    QVERIFY(service.quote(ticketId, departure.addDays(-8), &quote));
    QCOMPARE(quote.ratePercent, 0);
    QVERIFY(service.quote(ticketId, departure.addSecs(-48 * 60 * 60), &quote));
    QCOMPARE(quote.ratePercent, 5);
    QVERIFY(service.quote(ticketId, departure.addSecs(-24 * 60 * 60), &quote));
    QCOMPARE(quote.ratePercent, 10);
    QVERIFY(service.quote(ticketId, departure.addSecs(-1), &quote));
    QCOMPARE(quote.ratePercent, 20);

    domain::AppData candidate = store->data();
    candidate.tickets[0].priceCents = 150;
    QVERIFY(store->commit(candidate));
    QVERIFY(service.quote(ticketId, departure.addSecs(-48 * 60 * 60), &quote));
    QCOMPARE(quote.feeCents, qint64(8));
    QCOMPARE(quote.refundAmountCents, qint64(142));
}

void PhaseFiveTests::refundRestoresEverySegmentAndPersistsRecord()
{
    QTemporaryDir directory;
    const QDate serviceDate = QDate::currentDate().addDays(10);
    auto store = initializedStore(directory.filePath(QStringLiteral("app.json")), serviceDate);
    QVERIFY(store);
    const int firstBefore = store->data().trains[0].seats[0].segments[0].remainingSeats;
    const int secondBefore = store->data().trains[0].seats[0].segments[1].remainingSeats;
    const QString ticketId = bookOne(store.get(), serviceDate);
    QVERIFY(!ticketId.isEmpty());

    RefundReceipt receipt;
    QVERIFY(RefundService(store.get()).refund(ticketId,
                                               QDateTime(serviceDate.addDays(-9), QTime(8, 0)),
                                               &receipt));
    QCOMPARE(receipt.ratePercent, 0);
    QCOMPARE(store->data().tickets.first().status, domain::TicketStatus::Refunded);
    QCOMPARE(store->data().refunds.size(), 1);
    QCOMPARE(store->data().trains[0].seats[0].segments[0].remainingSeats, firstBefore);
    QCOMPARE(store->data().trains[0].seats[0].segments[1].remainingSeats, secondBefore);
    QVERIFY(store->reload());
    QCOMPARE(store->data().tickets.first().status, domain::TicketStatus::Refunded);
    QCOMPARE(store->data().refunds.first().ticketId, ticketId);
}

void PhaseFiveTests::departedAndRepeatedTicketsCannotBeRefunded()
{
    QTemporaryDir directory;
    const QDate serviceDate = QDate::currentDate().addDays(3);
    auto store = initializedStore(directory.filePath(QStringLiteral("app.json")), serviceDate);
    QVERIFY(store);
    const QString ticketId = bookOne(store.get(), serviceDate);
    QVERIFY(!ticketId.isEmpty());
    RefundService service(store.get());
    const QDateTime departure(serviceDate, QTime(8, 0));

    QVERIFY(!service.refund(ticketId, departure));
    QVERIFY(service.refund(ticketId, departure.addDays(-2)));
    const int refundCount = store->data().refunds.size();
    QVERIFY(!service.refund(ticketId, departure.addDays(-2)));
    QCOMPARE(store->data().refunds.size(), refundCount);
}

void PhaseFiveTests::completedTicketStatusRefreshesAtArrival()
{
    QTemporaryDir directory;
    const QDate serviceDate = QDate::currentDate().addDays(1);
    auto store = initializedStore(directory.filePath(QStringLiteral("app.json")), serviceDate);
    QVERIFY(store);
    const QString ticketId = bookOne(store.get(), serviceDate);
    QVERIFY(!ticketId.isEmpty());

    QVERIFY(OrderService(store.get()).refreshCompletedTickets(
        QDateTime(serviceDate, QTime(9, 20))));
    QCOMPARE(store->data().tickets.first().status, domain::TicketStatus::Completed);
    QCOMPARE(OrderService(store.get()).summaries().first().status,
             domain::TicketStatus::Completed);
}

void PhaseFiveTests::saveFailureRollsBackRefundAndInventory()
{
    QTemporaryDir directory;
    const QDate serviceDate = QDate::currentDate().addDays(10);
    auto writable = initializedStore(directory.filePath(QStringLiteral("app.json")), serviceDate);
    QVERIFY(writable);
    const QString ticketId = bookOne(writable.get(), serviceDate);
    QVERIFY(!ticketId.isEmpty());
    const domain::AppData initial = writable->data();

    DataStore store(std::make_unique<FailingRepository>(initial));
    QVERIFY(store.initialize());
    QVERIFY(store.login(QStringLiteral("admin"), QStringLiteral("admin")));
    const int remainingBefore = store.data().trains[0].seats[0].segments[0].remainingSeats;
    QVERIFY(!RefundService(&store).refund(ticketId,
                                          QDateTime(serviceDate.addDays(-9), QTime(8, 0))));
    QCOMPARE(store.data().tickets.first().status, domain::TicketStatus::Issued);
    QCOMPARE(store.data().refunds.size(), 0);
    QCOMPARE(store.data().trains[0].seats[0].segments[0].remainingSeats, remainingBefore);
}

QTEST_GUILESS_MAIN(PhaseFiveTests)

#include "phase5_tests.moc"

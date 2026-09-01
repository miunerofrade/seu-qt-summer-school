#include "data/datastore.h"
#include "data/idatarepository.h"
#include "data/jsonrepository.h"
#include "services/bookingservice.h"

#include <QTemporaryDir>
#include <QtTest>

#include <memory>

namespace {
std::unique_ptr<DataStore> initializedStore(const QString &path)
{
    auto store = std::make_unique<DataStore>(std::make_unique<JsonRepository>(path));
    if (!store->initialize())
        return {};
    return store;
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
}

class PhaseFourTests final : public QObject
{
    Q_OBJECT

private slots:
    void successfulBookingCreatesOrderTicketsAndDeductsSegments();
    void insufficientSeatsLeavesEverythingUnchanged();
    void duplicatePassengersAreRejected();
    void saveFailureRollsBackMemoryState();
};

void PhaseFourTests::successfulBookingCreatesOrderTicketsAndDeductsSegments()
{
    QTemporaryDir directory;
    auto store = initializedStore(directory.filePath(QStringLiteral("app.json")));
    QVERIFY(store);
    const QString passengerId = store->data().passengers.first().id;
    BookingReceipt receipt;
    const OperationResult result = BookingService(store.get()).book(
        {QStringLiteral("G101"), QDate::currentDate(), QStringLiteral("NJN"), QStringLiteral("SHH"),
         QStringLiteral("二等座"), {passengerId}},
        &receipt);
    QVERIFY(result);
    QVERIFY(!receipt.orderId.isEmpty());
    QCOMPARE(receipt.ticketIds.size(), 1);
    QCOMPARE(receipt.totalAmountCents, qint64(15000));
    QCOMPARE(store->data().orders.size(), 1);
    QCOMPARE(store->data().tickets.size(), 1);
    QCOMPARE(store->data().trains.first().seats.first().segments.at(0).remainingSeats, 39);
    QCOMPARE(store->data().trains.first().seats.first().segments.at(1).remainingSeats, 34);
    QVERIFY(store->reload());
    QCOMPARE(store->data().orders.size(), 1);
    QCOMPARE(store->data().tickets.size(), 1);
    QCOMPARE(store->data().trains.first().seats.first().segments.at(1).remainingSeats, 34);
}

void PhaseFourTests::insufficientSeatsLeavesEverythingUnchanged()
{
    QTemporaryDir directory;
    auto store = initializedStore(directory.filePath(QStringLiteral("app.json")));
    QVERIFY(store);
    auto candidate = store->data();
    const QString firstPassenger = candidate.passengers.first().id;
    domain::Passenger second{QStringLiteral("second"), QStringLiteral("第二位"), QStringLiteral("身份证"), QStringLiteral("320101199001011235")};
    candidate.passengers.push_back(second);
    candidate.trains[0].seats[0].segments[0].remainingSeats = 1;
    candidate.trains[0].seats[0].segments[1].remainingSeats = 1;
    QVERIFY(store->commit(candidate));

    const OperationResult result = BookingService(store.get()).book(
        {QStringLiteral("G101"), QDate::currentDate(), QStringLiteral("NJN"), QStringLiteral("SHH"),
         QStringLiteral("二等座"), {firstPassenger, second.id}});
    QVERIFY(!result);
    QCOMPARE(store->data().orders.size(), 0);
    QCOMPARE(store->data().tickets.size(), 0);
    QCOMPARE(store->data().trains[0].seats[0].segments[0].remainingSeats, 1);
    QCOMPARE(store->data().trains[0].seats[0].segments[1].remainingSeats, 1);
}

void PhaseFourTests::duplicatePassengersAreRejected()
{
    QTemporaryDir directory;
    auto store = initializedStore(directory.filePath(QStringLiteral("app.json")));
    QVERIFY(store);
    const QString passengerId = store->data().passengers.first().id;
    const OperationResult result = BookingService(store.get()).book(
        {QStringLiteral("G101"), QDate::currentDate(), QStringLiteral("NJN"), QStringLiteral("SHH"),
         QStringLiteral("二等座"), {passengerId, passengerId}});
    QVERIFY(!result);
    QCOMPARE(store->data().orders.size(), 0);
    QCOMPARE(store->data().tickets.size(), 0);
}

void PhaseFourTests::saveFailureRollsBackMemoryState()
{
    const domain::AppData initial = domain::createDemoData(QDate::currentDate());
    const QString passengerId = initial.passengers.first().id;
    DataStore store(std::make_unique<FailingRepository>(initial));
    QVERIFY(store.initialize());
    const int remainingBefore = store.data().trains.first().seats.first().segments.first().remainingSeats;
    const OperationResult result = BookingService(&store).book(
        {QStringLiteral("G101"), QDate::currentDate(), QStringLiteral("NJN"), QStringLiteral("SHH"),
         QStringLiteral("二等座"), {passengerId}});
    QVERIFY(!result);
    QCOMPARE(store.data().orders.size(), 0);
    QCOMPARE(store.data().tickets.size(), 0);
    QCOMPARE(store.data().trains.first().seats.first().segments.first().remainingSeats, remainingBefore);
}

QTEST_GUILESS_MAIN(PhaseFourTests)
#include "phase4_tests.moc"

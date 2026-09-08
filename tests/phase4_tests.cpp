#include "data/datastore.h"
#include "data/idatarepository.h"
#include "data/jsonrepository.h"
#include "features/booking/bookingservice.h"

#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>
#include <memory>

namespace {
std::unique_ptr<DataStore> initializedStore(const QString &path)
{
    auto store = std::make_unique<DataStore>(std::make_unique<JsonRepository>(path));
    if (!store->initialize() || !store->login(QStringLiteral("admin"), QStringLiteral("admin")))
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
    void demoBookingImportsOnlineTrainAndCreatesOrder();
    void fourStationSegmentPurchasesBlockLongRoute();
    void railwayIdentitySharesSegmentsAcrossDisplayNumberChange();
    void recurringCustomTrainHasIndependentDailyInventory();
    void bothBookingPathsRejectInvalidPassengersBeforeSaving();
    void demoRevalidatesAfterImportNotification();
};

void PhaseFourTests::successfulBookingCreatesOrderTicketsAndDeductsSegments()
{
    QTemporaryDir directory;
    auto store = initializedStore(directory.filePath(QStringLiteral("app.json")));
    QVERIFY(store);
    const QString passengerId = store->data().passengers.first().id;
    BookingReceipt receipt;
    const OperationResult result = BookingService(*store).book(
        {QStringLiteral("G101"), QDate::currentDate(), QStringLiteral("NKH"), QStringLiteral("AOH"),
         QStringLiteral("二等座"), {passengerId}},
        &receipt);
    QVERIFY(result);
    QVERIFY(!receipt.orderId.isEmpty());
    QCOMPARE(receipt.ticketIds.size(), 1);
    QCOMPARE(receipt.totalAmountCents, qint64(15000));
    QCOMPARE(store->data().orders.size(), 1);
    QCOMPARE(store->data().tickets.size(), 1);
    const auto occurrenceForToday = [&]() {
        return std::find_if(store->data().trains.cbegin(), store->data().trains.cend(), [](const domain::Train &train) {
            return train.number == QStringLiteral("G101")
                && train.railwayServiceDate == QDate::currentDate();
        });
    };
    auto occurrence = occurrenceForToday();
    QVERIFY(occurrence != store->data().trains.cend());
    const auto remainingSecondClass = [](const domain::Train &train, int segment) {
        const auto seat = std::find_if(train.seats.cbegin(), train.seats.cend(), [](const domain::SeatInventory &item) {
            return item.seatType == QStringLiteral("二等座");
        });
        return seat == train.seats.cend() ? -1 : seat->segments.at(segment).remainingSeats;
    };
    QCOMPARE(remainingSecondClass(*occurrence, 0), 39);
    QCOMPARE(remainingSecondClass(*occurrence, 1), 34);
    QVERIFY(store->reload());
    QCOMPARE(store->data().orders.size(), 1);
    QCOMPARE(store->data().tickets.size(), 1);
    occurrence = occurrenceForToday();
    QVERIFY(occurrence != store->data().trains.cend());
    QCOMPARE(remainingSecondClass(*occurrence, 1), 34);
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
    candidate.trains[0].seats[1].segments[0].remainingSeats = 1;
    candidate.trains[0].seats[1].segments[1].remainingSeats = 1;
    QVERIFY(store->commit(candidate));

    const OperationResult result = BookingService(*store).book(
        {QStringLiteral("G101"), QDate::currentDate(), QStringLiteral("NKH"), QStringLiteral("AOH"),
         QStringLiteral("二等座"), {firstPassenger, second.id}});
    QVERIFY(!result);
    QCOMPARE(store->data().orders.size(), 0);
    QCOMPARE(store->data().tickets.size(), 0);
    QCOMPARE(store->data().trains[0].seats[1].segments[0].remainingSeats, 1);
    QCOMPARE(store->data().trains[0].seats[1].segments[1].remainingSeats, 1);
}

void PhaseFourTests::duplicatePassengersAreRejected()
{
    QTemporaryDir directory;
    auto store = initializedStore(directory.filePath(QStringLiteral("app.json")));
    QVERIFY(store);
    const QString passengerId = store->data().passengers.first().id;
    const OperationResult result = BookingService(*store).book(
        {QStringLiteral("G101"), QDate::currentDate(), QStringLiteral("NKH"), QStringLiteral("AOH"),
         QStringLiteral("二等座"), {passengerId, passengerId}});
    QVERIFY(!result);
    QCOMPARE(store->data().orders.size(), 0);
    QCOMPARE(store->data().tickets.size(), 0);
}

void PhaseFourTests::saveFailureRollsBackMemoryState()
{
    const domain::AppData initial = domain::createDemoData();
    const QString passengerId = initial.passengers.first().id;
    DataStore store(std::make_unique<FailingRepository>(initial));
    QVERIFY(store.initialize());
    QVERIFY(store.login(QStringLiteral("admin"), QStringLiteral("admin")));
    const int remainingBefore = store.data().trains.first().seats.first().segments.first().remainingSeats;
    const OperationResult result = BookingService(store).book(
        {QStringLiteral("G101"), QDate::currentDate(), QStringLiteral("NKH"), QStringLiteral("AOH"),
         QStringLiteral("二等座"), {passengerId}});
    QVERIFY(!result);
    QCOMPARE(store.data().orders.size(), 0);
    QCOMPARE(store.data().tickets.size(), 0);
    QCOMPARE(store.data().trains.first().seats.first().segments.first().remainingSeats, remainingBefore);
}

void PhaseFourTests::demoBookingImportsOnlineTrainAndCreatesOrder()
{
    QTemporaryDir directory;
    auto store = initializedStore(directory.filePath(QStringLiteral("app.json")));
    QVERIFY(store);
    const QString passengerId = store->data().passengers.first().id;
    const QDate date = QDate::currentDate().addDays(1);
    BookingReceipt receipt;

    const OperationResult result = BookingService(*store).bookDemo(
        {QStringLiteral("G999"), date, QStringLiteral("NKH"), QStringLiteral("AOH"),
         QStringLiteral("二等座"), {passengerId}},
        {QStringLiteral("南京南"), QStringLiteral("上海虹桥"), QTime(10, 0), QTime(11, 20),
         0, 0, 16200, 20},
        &receipt);

    QVERIFY(result);
    QCOMPARE(receipt.totalAmountCents, qint64(16200));
    QCOMPARE(store->data().orders.size(), 1);
    QCOMPARE(store->data().tickets.size(), 1);
    const auto train = std::find_if(store->data().trains.cbegin(), store->data().trains.cend(),
                                    [&](const domain::Train &item) {
        return item.number == QStringLiteral("G999");
    });
    QVERIFY(train != store->data().trains.cend());
    QCOMPARE(train->stops.size(), 2);
    QCOMPARE(train->stops.first().stationCode, QStringLiteral("NKH"));
    QCOMPARE(train->stops.last().stationCode, QStringLiteral("AOH"));
    QCOMPARE(train->seats.first().segments.first().remainingSeats, 19);
    QVERIFY(store->reload());
    QCOMPARE(store->data().tickets.last().trainNumber, QStringLiteral("G999"));
}

void PhaseFourTests::fourStationSegmentPurchasesBlockLongRoute()
{
    QTemporaryDir directory;
    auto store = initializedStore(directory.filePath(QStringLiteral("app.json")));
    QVERIFY(store);
    domain::AppData candidate = store->data();
    for (int i = 1; i <= 4; ++i)
        candidate.stations.append({QStringLiteral("S%1").arg(i), QStringLiteral("站%1").arg(i),
                                   QStringLiteral("城市%1").arg(i), true});
    domain::Train train;
    train.number = QStringLiteral("T4");
    train.stops = {{QStringLiteral("S1"), 0, {}, QTime(8, 0)},
                   {QStringLiteral("S2"), 1, QTime(8, 20), QTime(8, 22)},
                   {QStringLiteral("S3"), 2, QTime(8, 40), QTime(8, 42)},
                   {QStringLiteral("S4"), 3, QTime(9, 0), {}}};
    train.seats = {{QStringLiteral("二等座"), {{100, 1, 1}, {100, 1, 1}, {100, 1, 1}}}};
    candidate.trains.append(train);
    QVERIFY(store->commit(candidate));
    const QString p1 = store->data().passengers.at(0).id;
    const QString p2 = store->data().passengers.at(1).id;
    QVERIFY(BookingService(*store).book(
        {QStringLiteral("T4"), QDate::currentDate(), QStringLiteral("S1"), QStringLiteral("S2"),
         QStringLiteral("二等座"), {p1}}));
    QVERIFY(BookingService(*store).book(
        {QStringLiteral("T4"), QDate::currentDate(), QStringLiteral("S3"), QStringLiteral("S4"),
         QStringLiteral("二等座"), {p2}}));
    QCOMPARE(store->data().tickets.at(0).seatId, QStringLiteral("01车01A"));
    QCOMPARE(store->data().tickets.at(1).seatId, QStringLiteral("01车01A"));
    QCOMPARE(store->data().trains.last().seats.first().details.first().occupiedMask, quint64(5));
    QCOMPARE(store->data().trains.last().seats.first().segments.at(0).remainingSeats, 0);
    QCOMPARE(store->data().trains.last().seats.first().segments.at(1).remainingSeats, 1);
    QCOMPARE(store->data().trains.last().seats.first().segments.at(2).remainingSeats, 0);
    QVERIFY(!BookingService(*store).book(
        {QStringLiteral("T4"), QDate::currentDate(), QStringLiteral("S1"), QStringLiteral("S4"),
         QStringLiteral("二等座"), {store->data().passengers.at(2).id}}));
    QCOMPARE(store->data().orders.size(), 2);
}

void PhaseFourTests::railwayIdentitySharesSegmentsAcrossDisplayNumberChange()
{
    QTemporaryDir directory;
    auto store = initializedStore(directory.filePath(QStringLiteral("app.json")));
    QVERIFY(store);
    const QDate date = QDate::currentDate().addDays(1);
    DemoTrainSnapshot snapshot{QStringLiteral("甲站"), QStringLiteral("丁站"),
                               QTime(8, 0), QTime(11, 0), 0, 0, 300, 1,
                               {{QStringLiteral("S1"), QStringLiteral("甲站"), {}, QTime(8, 0), 0},
                                {QStringLiteral("S2"), QStringLiteral("乙站"), QTime(9, 0), QTime(9, 5), 0},
                                {QStringLiteral("S3"), QStringLiteral("丙站"), QTime(10, 0), QTime(10, 5), 0},
                                {QStringLiteral("S4"), QStringLiteral("丁站"), QTime(11, 0), {}, 0}}};
    const QString serviceId = QStringLiteral("opaque-service-id");
    QVERIFY(BookingService(*store).bookDemo(
        {QStringLiteral("G100"), date, QStringLiteral("S1"), QStringLiteral("S2"),
         QStringLiteral("二等座"), {store->data().passengers.at(0).id}, serviceId}, snapshot));
    snapshot.departureStationName = QStringLiteral("丙站");
    snapshot.departureTime = QTime(10, 5);
    QVERIFY(BookingService(*store).bookDemo(
        {QStringLiteral("G101"), date, QStringLiteral("S3"), QStringLiteral("S4"),
         QStringLiteral("二等座"), {store->data().passengers.at(1).id}, serviceId}, snapshot));

    const auto service = std::find_if(store->data().trains.cbegin(), store->data().trains.cend(),
                                      [&serviceId, date](const domain::Train &train) {
        return train.railwayTrainId == serviceId && train.railwayServiceDate == date;
    });
    QVERIFY(service != store->data().trains.cend());
    QCOMPARE(service->seats.first().segments.at(0).remainingSeats, 0);
    QCOMPARE(service->seats.first().segments.at(1).remainingSeats, 1);
    QCOMPARE(service->seats.first().segments.at(2).remainingSeats, 0);
    QCOMPARE(store->data().tickets.last().trainNumber, QStringLiteral("G101"));
    QVERIFY(!BookingService(*store).book(
        {QStringLiteral("G100"), date, QStringLiteral("S1"), QStringLiteral("S4"),
         QStringLiteral("二等座"), {store->data().passengers.at(2).id}, serviceId}));
}

void PhaseFourTests::recurringCustomTrainHasIndependentDailyInventory()
{
    QTemporaryDir directory;
    auto store = initializedStore(directory.filePath(QStringLiteral("app.json")));
    QVERIFY(store);
    const QDate firstDate = QDate::currentDate().addDays(1);
    const QDate secondDate = firstDate.addDays(1);
    QVERIFY(BookingService(*store).book(
        {QStringLiteral("G101"), firstDate, QStringLiteral("NKH"), QStringLiteral("AOH"),
         QStringLiteral("二等座"), {store->data().passengers.at(0).id}}));
    QVERIFY(BookingService(*store).book(
        {QStringLiteral("G101"), secondDate, QStringLiteral("NKH"), QStringLiteral("AOH"),
         QStringLiteral("二等座"), {store->data().passengers.at(1).id}}));
    const auto remainingOn = [&](const QDate &date) {
        const auto occurrence = std::find_if(store->data().trains.cbegin(), store->data().trains.cend(),
                                             [&date](const domain::Train &train) {
            return train.number == QStringLiteral("G101") && train.railwayServiceDate == date;
        });
        if (occurrence == store->data().trains.cend())
            return -1;
        const auto seat = std::find_if(occurrence->seats.cbegin(), occurrence->seats.cend(), [](const domain::SeatInventory &item) {
            return item.seatType == QStringLiteral("二等座");
        });
        return seat == occurrence->seats.cend() ? -1 : seat->segments.first().remainingSeats;
    };
    QCOMPARE(remainingOn(firstDate), 39);
    QCOMPARE(remainingOn(secondDate), 39);
    QCOMPARE(store->data().trains.first().seats.at(1).segments.first().remainingSeats, 40);
}

void PhaseFourTests::bothBookingPathsRejectInvalidPassengersBeforeSaving()
{
    QTemporaryDir directory;
    auto store = initializedStore(directory.filePath(QStringLiteral("app.json")));
    QVERIFY(store);
    QVERIFY(store->registerUser("alice", "a"));
    QVERIFY(store->login("alice", "a"));
    auto data = store->data();
    data.passengers[0].ownerUserId = store->currentUserId();
    QVERIFY(store->commit(data));
    const QString own = data.passengers.at(0).id;
    const QString other = data.passengers.at(1).id;
    const DemoTrainSnapshot snapshot{"南京南", "上海虹桥", QTime(10, 0), QTime(11, 20),
                                      0, 0, 16200, 20};
    BookingService service(*store);
    QSignalSpy changed(store.get(), &DataStore::dataChanged);
    for (const QStringList &ids : {QStringList{own, own}, QStringList{other},
                                  QStringList{"missing"}, QStringList{}, QStringList{""}}) {
        const BookingRequest request{"G101", QDate::currentDate(), "NKH", "AOH", "二等座", ids};
        QVERIFY(!service.book(request));
        QVERIFY(!service.bookDemo(request, snapshot));
    }
    QCOMPARE(changed.count(), 0);
    store->logout();
    changed.clear(); // 注销本身会发出 dataChanged，以刷新账户范围内的视图。
    const BookingRequest request{"G101", QDate::currentDate(), "NKH", "AOH", "二等座", {own}};
    QVERIFY(!service.book(request));
    QVERIFY(!service.bookDemo(request, snapshot));
    QCOMPARE(changed.count(), 0);
    QCOMPARE(store->data().trains.size(), data.trains.size());
    QCOMPARE(store->data().orders.size(), data.orders.size());
    QCOMPARE(store->data().tickets.size(), data.tickets.size());
}

void PhaseFourTests::demoRevalidatesAfterImportNotification()
{
    QTemporaryDir directory;
    auto store = initializedStore(directory.filePath(QStringLiteral("app.json")));
    QVERIFY(store);
    const QString passenger = store->data().passengers.first().id;
    bool notified = false;
    QObject::connect(store.get(), &DataStore::dataChanged, store.get(), [&]() {
        if (notified) return;
        notified = true;
        store->logout();
    });
    const auto result = BookingService(*store).bookDemo(
        {"G999", QDate::currentDate(), "NKH", "AOH", "二等座", {passenger}},
        {"南京南", "上海虹桥", QTime(10, 0), QTime(11, 20), 0, 0, 16200, 20});
    QVERIFY(notified);
    QVERIFY(!result);
    QVERIFY(store->data().orders.isEmpty());
    QVERIFY(store->data().tickets.isEmpty());
}

QTEST_GUILESS_MAIN(PhaseFourTests)
#include "phase4_tests.moc"

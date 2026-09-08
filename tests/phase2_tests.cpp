#include "data/datastore.h"
#include "data/jsonrepository.h"
#include "features/passengers/passengerfilterproxymodel.h"
#include "features/passengers/passengertablemodel.h"
#include "features/admin/adminservice.h"
#include "features/passengers/passengerservice.h"

#include <QTemporaryDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include <algorithm>
#include <memory>

class PhaseTwoTests final : public QObject
{
    Q_OBJECT

private slots:
    void passengerValidationMaskingAndModel();
    void identityValidationAndNameLength();
    void activeTicketPreventsPassengerDeletion();
    void stationAndTrainCodesAreUnique();
    void scheduleValidationAndSeatReset();
    void seatInventoryValidation();
    void referencedBaseDataCannotBeDeleted();
    void officialStationsCannotBeDuplicatedByCodeOrName();
};

namespace {
std::unique_ptr<DataStore> initializedStore(const QString &path)
{
    auto store = std::make_unique<DataStore>(std::make_unique<JsonRepository>(path));
    if (!store->initialize() || !store->login(QStringLiteral("admin"), QStringLiteral("admin")))
        return {};
    return store;
}
} // 命名空间

void PhaseTwoTests::passengerValidationMaskingAndModel()
{
    QTemporaryDir directory;
    auto store = initializedStore(directory.filePath(QStringLiteral("app-data.json")));
    QVERIFY(store);
    PassengerService service(store.get());
    const int initialCount = store->data().passengers.size();

    QVERIFY(!service.addPassenger(QStringLiteral("  "), QStringLiteral("身份证"), QStringLiteral("1")));
    QVERIFY(!service.addPassenger(QStringLiteral("重复"),
                                  QStringLiteral("身份证"),
                                  store->data().passengers.first().documentNumber));
    QString createdId;
    QVERIFY(service.addPassenger(QStringLiteral("  李四 "),
                                 QStringLiteral("护照"),
                                 QStringLiteral(" E12345678 "),
                                 &createdId));
    QCOMPARE(store->data().passengers.size(), initialCount + 1);
    QCOMPARE(store->data().passengers.last().name, QStringLiteral("李四"));
    QCOMPARE(PassengerService::maskedDocumentNumber(QStringLiteral("320101199001010014")),
             QStringLiteral("3201**********0014"));

    PassengerTableModel model(store.get());
    PassengerFilterProxyModel proxy;
    proxy.setSourceModel(&model);
    QCOMPARE(proxy.rowCount(), initialCount + 1);
    proxy.setKeyword(QStringLiteral("E123"));
    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(proxy.index(0, PassengerTableModel::DocumentNumberColumn).data().toString(),
             QStringLiteral("E123*5678"));

    QVERIFY(service.removePassenger(createdId));
    QCOMPARE(store->data().passengers.size(), initialCount);
}

void PhaseTwoTests::identityValidationAndNameLength()
{
    QTemporaryDir directory;
    auto store = initializedStore(directory.filePath(QStringLiteral("app-data.json")));
    QVERIFY(store);
    PassengerService service(store.get());

    QVERIFY(service.addPassenger(QStringLiteral("张三"),
                                 QStringLiteral("身份证"),
                                 QStringLiteral("110105198001010008")));
    QVERIFY(service.addPassenger(QStringLiteral("李四"),
                                 QStringLiteral("身份证"),
                                 QStringLiteral("11010519810101003x")));
    QCOMPARE(store->data().passengers.last().documentNumber,
             QStringLiteral("11010519810101003X"));
    QVERIFY(!service.addPassenger(QStringLiteral("王五"),
                                  QStringLiteral("身份证"),
                                  QStringLiteral("110105198001010009")));
    QVERIFY(!service.addPassenger(QStringLiteral("赵六"),
                                  QStringLiteral("身份证"),
                                  QStringLiteral("110105198013010008")));
    QVERIFY(!service.addPassenger(QStringLiteral("周七"),
                                  QStringLiteral("身份证"),
                                  QStringLiteral("11010519800101000")));
    QVERIFY(!service.addPassenger(QString(21, QLatin1Char('a')),
                                  QStringLiteral("护照"),
                                  QStringLiteral("E12345678")));
}

void PhaseTwoTests::activeTicketPreventsPassengerDeletion()
{
    QTemporaryDir directory;
    auto store = initializedStore(directory.filePath(QStringLiteral("app-data.json")));
    QVERIFY(store);
    const QString passengerId = store->data().passengers.first().id;
    domain::AppData candidate = store->data();
    candidate.tickets.append({QStringLiteral("ticket-1"),
                              passengerId,
                              QStringLiteral("G101"),
                              QStringLiteral("NKH"),
                              QStringLiteral("AOH"),
                              QStringLiteral("二等座"),
                              15000,
                              domain::TicketStatus::Issued});
    QVERIFY(store->commit(candidate));

    PassengerService service(store.get());
    QVERIFY(!service.removePassenger(passengerId));
    QVERIFY(std::any_of(store->data().passengers.cbegin(),
                        store->data().passengers.cend(),
                        [&passengerId](const domain::Passenger &passenger) {
                            return passenger.id == passengerId;
                        }));
}

void PhaseTwoTests::stationAndTrainCodesAreUnique()
{
    QTemporaryDir directory;
    auto store = initializedStore(directory.filePath(QStringLiteral("app-data.json")));
    QVERIFY(store);
    AdminService service(store.get());

    QVERIFY(!service.addStation({QStringLiteral("nkh"), QStringLiteral("重复南京南"), QStringLiteral("南京"), true}));
    QVERIFY(service.addStation({QStringLiteral("WXH"), QStringLiteral("无锡东"), QStringLiteral("无锡"), true}));
    domain::Train duplicate;
    duplicate.number = QStringLiteral("g101");
    duplicate.stops = store->data().trains.first().stops;
    QVERIFY(!service.addTrain(duplicate));
    domain::Train added;
    added.number = QStringLiteral("G999");
    added.stops = store->data().trains.first().stops;
    QVERIFY(service.addTrain(added));
}

void PhaseTwoTests::scheduleValidationAndSeatReset()
{
    QTemporaryDir directory;
    auto store = initializedStore(directory.filePath(QStringLiteral("app-data.json")));
    QVERIFY(store);
    AdminService service(store.get());

    QVector<domain::TrainStop> duplicateStops{
        {QStringLiteral("NKH"), 0, {}, QTime(8, 0), 0},
        {QStringLiteral("NKH"), 1, QTime(9, 0), {}, 0}};
    QVERIFY(!service.replaceStops(QStringLiteral("G101"), duplicateStops));

    QVector<domain::TrainStop> validStops{
        {QStringLiteral("NKH"), 0, {}, QTime(8, 0), 0},
        {QStringLiteral("AOH"), 1, QTime(9, 20), {}, 0}};
    QVERIFY(service.replaceStops(QStringLiteral("G101"), validStops));
    const auto &train = store->data().trains.first();
    QCOMPARE(train.stops.size(), 2);
    QCOMPARE(train.seats.size(), 2);
    const auto secondClass = std::find_if(train.seats.cbegin(), train.seats.cend(), [](const domain::SeatInventory &seat) {
        return seat.seatType == QStringLiteral("二等座");
    });
    QVERIFY(secondClass != train.seats.cend());
    QCOMPARE(secondClass->segments.size(), 1);
    QCOMPARE(secondClass->segments.first().priceCents, qint64(15000));
    QCOMPARE(secondClass->segments.first().remainingSeats, 35);

    QVector<domain::TrainStop> expandedStops{
        {QStringLiteral("NKH"), 0, {}, QTime(8, 0), 0},
        {QStringLiteral("OHH"), 1, QTime(8, 45), QTime(8, 47), 0},
        {QStringLiteral("AOH"), 2, QTime(9, 20), {}, 0}};
    QVERIFY(service.replaceStops(QStringLiteral("G101"), expandedStops));
    const auto &expandedSeats = store->data().trains.first().seats;
    const auto expandedSeat = std::find_if(expandedSeats.cbegin(), expandedSeats.cend(), [](const domain::SeatInventory &seat) {
        return seat.seatType == QStringLiteral("二等座");
    });
    QVERIFY(expandedSeat != expandedSeats.cend());
    QCOMPARE(expandedSeat->segments.size(), 2);
    QCOMPARE(expandedSeat->segments.at(0).priceCents + expandedSeat->segments.at(1).priceCents,
             qint64(15000));
    QCOMPARE(expandedSeat->segments.at(0).remainingSeats, 35);
    QCOMPARE(expandedSeat->segments.at(1).remainingSeats, 35);
}

void PhaseTwoTests::seatInventoryValidation()
{
    QTemporaryDir directory;
    auto store = initializedStore(directory.filePath(QStringLiteral("app-data.json")));
    QVERIFY(store);
    AdminService service(store.get());

    QVector<domain::SeatInventory> invalid{{QStringLiteral("二等座"), {{1000, 10, 11}, {1000, 10, 9}}}};
    QVERIFY(!service.replaceSeats(QStringLiteral("G101"), invalid));
    QVector<domain::SeatInventory> valid{{QStringLiteral("商务座"), {{20000, 10, 8}, {18000, 10, 7}}}};
    QVERIFY(service.replaceSeats(QStringLiteral("G101"), valid));
    QCOMPARE(store->data().trains.first().seats.first().seatType, QStringLiteral("商务座"));
    QCOMPARE(store->data().trains.first().seats.first().details.size(), 10);
    QCOMPARE(domain::availableSeatCount(store->data().trains.first().seats.first(),
                                        domain::segmentMask(0, 2)), 7);
    auto details = store->data().trains.first().seats.first().details;
    for (domain::SeatDetail &detail : details)
        detail.occupiedMask = 0;
    QVERIFY(service.replaceSeatDetails(QStringLiteral("G101"), QStringLiteral("商务座"), details));
    QCOMPARE(store->data().trains.first().seats.first().segments.first().remainingSeats, 10);
    details.first().occupiedMask = quint64(1) << 10;
    QVERIFY(!service.replaceSeatDetails(QStringLiteral("G101"), QStringLiteral("商务座"), details));
}

void PhaseTwoTests::referencedBaseDataCannotBeDeleted()
{
    QTemporaryDir directory;
    auto store = initializedStore(directory.filePath(QStringLiteral("app-data.json")));
    QVERIFY(store);
    AdminService service(store.get());

    QVERIFY(!service.removeStation(QStringLiteral("NKH")));
    domain::AppData candidate = store->data();
    candidate.tickets.append({QStringLiteral("ticket-2"),
                              candidate.passengers.first().id,
                              QStringLiteral("G101"),
                              QStringLiteral("NKH"),
                              QStringLiteral("AOH"),
                              QStringLiteral("二等座"),
                              15000,
                              domain::TicketStatus::Completed});
    QVERIFY(store->commit(candidate));
    QVERIFY(service.removeTrain(QStringLiteral("G101")));
    QVERIFY(store->data().hiddenTrainNumbers.contains(QStringLiteral("G101")));
    QVERIFY(service.restoreTrain(QStringLiteral("g101")));
    QVERIFY(!store->data().hiddenTrainNumbers.contains(QStringLiteral("G101"), Qt::CaseInsensitive));
    QVERIFY(!service.restoreTrain(QStringLiteral("G101")));
    QVERIFY(service.updateStation(QStringLiteral("NKH"), QStringLiteral("南京南"), QStringLiteral("南京"), false));
}

void PhaseTwoTests::officialStationsCannotBeDuplicatedByCodeOrName()
{
    QTemporaryDir directory;
    auto store = initializedStore(directory.filePath(QStringLiteral("app-data.json")));
    QVERIFY(store);
    QFile catalog(directory.filePath(QStringLiteral("railway-stations.json")));
    QVERIFY(catalog.open(QIODevice::WriteOnly));
    catalog.write(QJsonDocument(QJsonObject{
        {QStringLiteral("stations"),
         QJsonArray{QJsonObject{{QStringLiteral("code"), QStringLiteral("NKH")},
                                {QStringLiteral("name"), QStringLiteral("南京南")}}}}})
                      .toJson(QJsonDocument::Compact));
    catalog.close();

    AdminService service(store.get());
    OperationResult result = service.addStation(
        {QStringLiteral("NKH"), QStringLiteral("另一个名字"), QStringLiteral("南京"), true});
    QVERIFY(!result);
    QVERIFY(result.error.contains(QStringLiteral("不能创建")));
    result = service.addStation(
        {QStringLiteral("CUS"), QStringLiteral("南京南"), QStringLiteral("自定义"), true});
    QVERIFY(!result);
    QVERIFY(result.error.contains(QStringLiteral("NKH")));
    QVERIFY(service.addStation(
        {QStringLiteral("CUS"), QStringLiteral("自建站"), QStringLiteral("自定义"), true}));
    QVERIFY(!service.updateStation(QStringLiteral("CUS"), QStringLiteral("南京南"),
                                   QStringLiteral("自定义"), true));
}

QTEST_GUILESS_MAIN(PhaseTwoTests)

#include "phase2_tests.moc"

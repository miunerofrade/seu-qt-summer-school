#include "data/datastore.h"
#include "data/idatarepository.h"
#include "data/jsonrepository.h"
#include "domain/entities.h"
#include "common/money.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>
#include <utility>

class FailingRepository final : public IDataRepository
{
public:
    explicit FailingRepository(domain::AppData data)
        : m_data(std::move(data))
    {
    }

    LoadResult load() const override
    {
        return {true, false, m_data, {}};
    }

    OperationResult save(const domain::AppData &) const override
    {
        return OperationResult::failure(QStringLiteral("模拟写入失败"));
    }

    QString dataFilePath() const override
    {
        return QStringLiteral("/invalid/app-data.json");
    }

private:
    domain::AppData m_data;
};

class PhaseOneTests final : public QObject
{
    Q_OBJECT

private slots:
    void missingFileCreatesDemoData();
    void moneyFormatting();
    void jsonRoundTripPreservesDomainData();
    void corruptJsonIsNotOverwritten();
    void unsupportedVersionIsRejected();
    void failedCommitKeepsMemoryUnchanged();
    void backupRestoresDataAndKeepsSafetySnapshot();
};

void PhaseOneTests::missingFileCreatesDemoData()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("data/app-data.json"));

    DataStore store(std::make_unique<JsonRepository>(path));
    QVERIFY2(store.initialize(), qPrintable(store.errorMessage()));
    QVERIFY(QFileInfo::exists(path));
    QCOMPARE(store.loadState(), DataStore::LoadState::Ready);
    QVERIFY(store.isWritable());
    QVERIFY(store.data().stations.size() >= 3);
    QVERIFY(store.data().trains.size() >= 2);
    QVERIFY(!store.data().passengers.isEmpty());
    QCOMPARE(store.data().users.size(), 1);
    QVERIFY(store.login(QStringLiteral("admin"), QStringLiteral("admin")));
    QVERIFY(store.isAdmin());
    QSet<QString> passengerIds;
    for (const auto &passenger : store.data().passengers) {
        QCOMPARE(passenger.ownerUserId, store.currentUserId());
        QVERIFY(!passenger.id.isEmpty());
        QVERIFY(!passenger.name.isEmpty());
        QVERIFY(!passengerIds.contains(passenger.id));
        passengerIds.insert(passenger.id);
    }
    const auto originalIds = passengerIds;
    DataStore restarted(std::make_unique<JsonRepository>(path));
    QVERIFY(restarted.initialize());
    QVERIFY(restarted.login(QStringLiteral("admin"), QStringLiteral("admin")));
    passengerIds.clear();
    for (const auto &passenger : restarted.data().passengers)
        passengerIds.insert(passenger.id);
    QCOMPARE(passengerIds, originalIds);
    QCOMPARE(restarted.data().users.size(), 1);
}

void PhaseOneTests::moneyFormatting()
{
    QCOMPARE(common::formatMoney(0), QStringLiteral("¥0.00"));
    QCOMPARE(common::formatMoney(5), QStringLiteral("¥0.05"));
    QCOMPARE(common::formatMoney(12345), QStringLiteral("¥123.45"));
}

void PhaseOneTests::jsonRoundTripPreservesDomainData()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("app-data.json"));
    domain::AppData source = domain::createDemoData();
    source.hiddenTrainNumbers.append(QStringLiteral("G205"));

    JsonRepository repository(path);
    const OperationResult saved = repository.save(source);
    QVERIFY2(saved, qPrintable(saved.error));
    const LoadResult loaded = repository.load();
    QVERIFY2(loaded.success, qPrintable(loaded.error));
    QCOMPARE(loaded.data.schemaVersion, domain::CurrentSchemaVersion);
    QCOMPARE(loaded.data.stations.size(), source.stations.size());
    QCOMPARE(loaded.data.trains.size(), source.trains.size());
    QCOMPARE(loaded.data.trains.first().number, QStringLiteral("G101"));
    QCOMPARE(loaded.data.hiddenTrainNumbers, QStringList{QStringLiteral("G205")});
    QCOMPARE(loaded.data.trains.first().seats.first().seatType, QStringLiteral("一等座"));
    QCOMPARE(loaded.data.trains.first().seats.at(1).seatType, QStringLiteral("二等座"));
    QCOMPARE(loaded.data.trains.first().seats.first().segments.at(1).remainingSeats, 69);
    QCOMPARE(loaded.data.trains.first().seats.first().details.first().seatId,
             QStringLiteral("01车01A"));
    QCOMPARE(loaded.data.trains.first().seats.at(1).details.first().seatId,
             QStringLiteral("03车01A"));
    QVERIFY(!loaded.data.trains.first().seats.first().details.isEmpty());
    QFile jsonFile(path);
    QVERIFY(jsonFile.open(QIODevice::ReadOnly));
    const QJsonObject root = QJsonDocument::fromJson(jsonFile.readAll()).object();
    const QJsonObject detail = root.value("trains").toArray().first().toObject()
        .value("seats").toArray().first().toObject()
        .value("details").toArray().first().toObject();
    QVERIFY(detail.value("occupiedMask").isString());
}

void PhaseOneTests::corruptJsonIsNotOverwritten()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("app-data.json"));
    const QByteArray corruptContent("{ this is not valid json");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(corruptContent), corruptContent.size());
    file.close();

    DataStore store(std::make_unique<JsonRepository>(path));
    QVERIFY(!store.initialize());
    QCOMPARE(store.loadState(), DataStore::LoadState::Error);
    QVERIFY(!store.isWritable());
    QVERIFY(!store.save());

    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), corruptContent);
}

void PhaseOneTests::unsupportedVersionIsRejected()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("app-data.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("{\"schemaVersion\":99}");
    file.close();

    const LoadResult loaded = JsonRepository(path).load();
    QVERIFY(!loaded.success);
    QVERIFY(!loaded.fileMissing);
    QVERIFY(loaded.error.contains(QStringLiteral("不支持的数据版本")));
}

void PhaseOneTests::failedCommitKeepsMemoryUnchanged()
{
    const domain::AppData original = domain::createDemoData();
    DataStore store(std::make_unique<FailingRepository>(original));
    QVERIFY(store.initialize());

    domain::AppData candidate = store.data();
    candidate.stations.clear();
    const OperationResult result = store.commit(candidate);
    QVERIFY(!result);
    QCOMPARE(store.data().stations.size(), original.stations.size());
}

void PhaseOneTests::backupRestoresDataAndKeepsSafetySnapshot()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("data/app-data.json"));
    DataStore store(std::make_unique<JsonRepository>(path));
    QVERIFY(store.initialize());
    const int originalStationCount = store.data().stations.size();

    QVERIFY(store.ensureBackup());
    QVERIFY(QFileInfo::exists(store.backupFilePath()));

    domain::AppData changed = store.data();
    changed.stations.append({QStringLiteral("TST"), QStringLiteral("测试站"), QStringLiteral("测试"), true});
    QVERIFY(store.commit(changed));
    QCOMPARE(store.data().stations.size(), originalStationCount + 1);

    QVERIFY(store.restoreBackup());
    QCOMPARE(store.data().stations.size(), originalStationCount);
    QVERIFY(!store.lastSafetyBackupPath().isEmpty());
    QVERIFY(QFileInfo::exists(store.lastSafetyBackupPath()));

    const LoadResult safety = JsonRepository(store.lastSafetyBackupPath()).load();
    QVERIFY(safety.success);
    QCOMPARE(safety.data.stations.size(), originalStationCount + 1);
}

QTEST_GUILESS_MAIN(PhaseOneTests)

#include "phase1_tests.moc"

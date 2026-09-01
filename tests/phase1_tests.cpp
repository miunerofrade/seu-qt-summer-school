#include "data/datastore.h"
#include "data/idatarepository.h"
#include "data/jsonrepository.h"
#include "domain/entities.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

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
    void jsonRoundTripPreservesDomainData();
    void corruptJsonIsNotOverwritten();
    void unsupportedVersionIsRejected();
    void failedCommitKeepsMemoryUnchanged();
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
}

void PhaseOneTests::jsonRoundTripPreservesDomainData()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("app-data.json"));
    const domain::AppData source = domain::createDemoData(QDate(2026, 9, 1));

    JsonRepository repository(path);
    const OperationResult saved = repository.save(source);
    QVERIFY2(saved, qPrintable(saved.error));
    const LoadResult loaded = repository.load();
    QVERIFY2(loaded.success, qPrintable(loaded.error));
    QCOMPARE(loaded.data.schemaVersion, domain::CurrentSchemaVersion);
    QCOMPARE(loaded.data.stations.size(), source.stations.size());
    QCOMPARE(loaded.data.trains.size(), source.trains.size());
    QCOMPARE(loaded.data.trains.first().number, QStringLiteral("G101"));
    QCOMPARE(loaded.data.trains.first().serviceDate, QDate(2026, 9, 1));
    QCOMPARE(loaded.data.trains.first().seats.first().segments.at(1).remainingSeats, 35);
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
    const domain::AppData original = domain::createDemoData(QDate(2026, 9, 1));
    DataStore store(std::make_unique<FailingRepository>(original));
    QVERIFY(store.initialize());

    domain::AppData candidate = store.data();
    candidate.stations.clear();
    const OperationResult result = store.commit(candidate);
    QVERIFY(!result);
    QCOMPARE(store.data().stations.size(), original.stations.size());
}

QTEST_GUILESS_MAIN(PhaseOneTests)

#include "phase1_tests.moc"

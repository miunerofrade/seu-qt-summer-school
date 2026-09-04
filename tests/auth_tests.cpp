#include "app/mainwindow.h"
#include "data/datastore.h"
#include "data/jsonrepository.h"
#include "features/auth/logindialog.h"
#include "models/passengertablemodel.h"
#include "models/ordertablemodel.h"
#include "services/bookingservice.h"
#include "services/orderservice.h"
#include "services/passengerservice.h"
#include "services/refundservice.h"
#include "services/railwayqueryservice.h"
#include "services/statisticsservice.h"

#include <QAction>
#include <QDateEdit>
#include <QComboBox>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableView>
#include <QTemporaryDir>
#include <QToolButton>
#include <QVariantAnimation>
#include <QtTest>
#include <memory>

namespace {
class FailingRepository final : public IDataRepository
{
public:
    LoadResult load() const override { return {true, false, domain::createDemoData(), {}}; }
    OperationResult save(const domain::AppData &) const override
    { return OperationResult::failure(QStringLiteral("模拟保存失败")); }
    QString dataFilePath() const override { return QStringLiteral("memory://auth"); }
};

BookingRequest requestFor(const QString &passenger)
{
    return {QStringLiteral("G101"), QDate::currentDate().addDays(10), QStringLiteral("NKH"),
            QStringLiteral("AOH"), QStringLiteral("二等座"), {passenger}};
}
}

class AuthTests final : public QObject
{
    Q_OBJECT
private slots:
    void registrationPersistenceAndFailure();
    void legacyMigrationAndBackup();
    void ownersAndSharedInventory();
    void loginFormAndAnimation();
    void lastLoginPrefill();
    void loginQuitShortcut();
    void roleNavigationAndLogout();
    void startupWithFullStationCatalog();
    void stationCatalogFreshness();
};

void AuthTests::registrationPersistenceAndFailure()
{
    QTemporaryDir dir;
    DataStore store(std::make_unique<JsonRepository>(dir.filePath("app.json")));
    QVERIFY(store.initialize());
    QVERIFY(!store.isAdmin());
    QVERIFY(store.login(" ADMIN ", "admin"));
    QVERIFY(store.isAdmin());
    QVERIFY(!store.login("admin", "bad"));
    QVERIFY(store.currentUserId().isEmpty());
    QVERIFY(!store.registerUser(" ", "password"));
    QVERIFY(!store.registerUser("Alice", ""));
    QVERIFY(store.registerUser(" Alice ", " plain password "));
    QVERIFY(!store.registerUser("ALICE", "another"));
    QVERIFY(store.login("alice", " plain password "));
    const QString alice = store.currentUserId();
    QVERIFY(!store.isAdmin());
    QVERIFY(store.reload());
    QCOMPARE(store.usernameFor(alice), QStringLiteral("Alice"));
    DataStore reopened(std::make_unique<JsonRepository>(dir.filePath("app.json")));
    QVERIFY(reopened.initialize());
    QVERIFY(reopened.login("Alice", " plain password "));
    QCOMPARE(reopened.currentUserId(), alice);
    auto candidate = reopened.data();
    candidate.users[0].password = QStringLiteral("changed");
    QVERIFY(reopened.commit(candidate));
    QVERIFY(reopened.reload());
    QVERIFY(!reopened.login("admin", "admin"));
    QVERIFY(reopened.login("admin", "changed"));
    DataStore failing(std::make_unique<FailingRepository>());
    QVERIFY(failing.initialize());
    QVERIFY(!failing.registerUser("new", "password"));
    QCOMPARE(failing.data().users.size(), 1);
    QVERIFY(!failing.login("new", "password"));
}

void AuthTests::legacyMigrationAndBackup()
{
    QTemporaryDir dir;
    const QString path = dir.filePath("app.json");
    QVERIFY(JsonRepository(path).save(domain::createDemoData()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    auto json = QJsonDocument::fromJson(file.readAll()).object();
    file.close();
    json.insert("schemaVersion", 3);
    json.remove("users");
    QJsonArray passengers;
    for (const auto &entry : json.value("passengers").toArray()) {
        auto passenger = entry.toObject();
        passenger.remove("ownerUserId");
        passengers.append(passenger);
    }
    json.insert("passengers", passengers);
    json.insert("orders", QJsonArray{QJsonObject{{"id", "legacy-order"},
        {"createdAt", QDateTime::currentDateTime().toString(Qt::ISODate)},
        {"ticketIds", QJsonArray{}}, {"totalAmountCents", 0}}});
    const QByteArray legacy = QJsonDocument(json).toJson();
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(file.write(legacy), legacy.size());
    file.close();
    DataStore store(std::make_unique<JsonRepository>(path));
    QVERIFY(store.initialize());
    QCOMPARE(store.data().schemaVersion, 4);
    QCOMPARE(JsonRepository(path).load().data.schemaVersion, 4);
    QVERIFY(store.login("admin", "admin"));
    QCOMPARE(store.data().passengers.first().ownerUserId, store.currentUserId());
    QCOMPARE(store.data().orders.first().ownerUserId, store.currentUserId());
    QFile backup(store.backupFilePath());
    QVERIFY(backup.open(QIODevice::WriteOnly));
    QCOMPARE(backup.write(legacy), legacy.size());
    backup.close();
    QVERIFY(store.registerUser("Alice", "password"));
    QVERIFY(store.restoreBackup());
    QCOMPARE(store.data().users.size(), 2);
    QVERIFY(store.login("Alice", "password"));
    QVERIFY(OrderService(&store).summaries().isEmpty());
}

void AuthTests::ownersAndSharedInventory()
{
    QTemporaryDir dir;
    DataStore store(std::make_unique<JsonRepository>(dir.filePath("app.json")));
    QVERIFY(store.initialize());
    QVERIFY(store.registerUser("alice", "a"));
    QVERIFY(store.registerUser("bob", "b"));
    QVERIFY(store.login("alice", "a"));
    const QString alice = store.currentUserId();
    QString alicePassenger;
    PassengerService passengers(&store);
    QVERIFY(passengers.addPassenger("Alice", "护照", "P001", &alicePassenger));
    QVERIFY(!passengers.addPassenger("Duplicate", "护照", "P001"));
    BookingReceipt aliceOrder;
    QVERIFY(BookingService(&store).book(requestFor(alicePassenger), &aliceOrder));
    QCOMPARE(store.data().orders.last().ownerUserId, alice);
    QCOMPARE(OrderService(&store).summaries().size(), 1);
    QVERIFY(store.login("bob", "b"));
    QVERIFY(OrderService(&store).summaries().isEmpty());
    QVERIFY(OrderService(&store).details(aliceOrder.orderId).isEmpty());
    QVERIFY(!passengers.updatePassenger(alicePassenger, "Hacked", "护照", "OTHER"));
    QVERIFY(!passengers.removePassenger(alicePassenger));
    QVERIFY(!BookingService(&store).book(requestFor(alicePassenger)));
    RefundQuote quote;
    QVERIFY(!RefundService(&store).quote(aliceOrder.ticketIds.first(), QDateTime::currentDateTime(), &quote));
    QVERIFY(!RefundService(&store).refund(aliceOrder.ticketIds.first(), QDateTime::currentDateTime()));
    QString bobPassenger;
    QVERIFY(passengers.addPassenger("Bob", "护照", "P001", &bobPassenger));
    PassengerTableModel model(&store);
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.passengerAt(0)->id, bobPassenger);
    BookingReceipt bobOrder;
    QVERIFY(BookingService(&store).book(requestFor(bobPassenger), &bobOrder));
    QCOMPARE(OrderService(&store).summaries().size(), 1);
    for (const auto &train : store.data().trains) {
        if (train.number == "G101" && train.railwayServiceDate == requestFor(bobPassenger).serviceDate) {
            QCOMPARE(train.seats.first().segments[0].remainingSeats, 38);
            QCOMPARE(train.seats.first().segments[1].remainingSeats, 33);
        }
    }
    QVERIFY(store.login("admin", "admin"));
    QCOMPARE(OrderService(&store).summaries().size(), 2);
    QCOMPARE(OrderService(&store).details(aliceOrder.orderId).size(), 1);
    QVERIFY(passengers.updatePassenger(alicePassenger, "Alice updated", "护照", "P001"));
    QVERIFY(RefundService(&store).refund(aliceOrder.ticketIds.first(), QDateTime::currentDateTime()));
    QVERIFY(passengers.removePassenger(alicePassenger));
    OrderTableModel orders(&store);
    QCOMPARE(orders.rowCount(), 2);
    QVERIFY(orders.searchableText(0).contains("alice") || orders.searchableText(1).contains("alice"));
    store.logout();
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(orders.rowCount(), 0);
    QVERIFY(!BookingService(&store).book(requestFor(bobPassenger)));
}

void AuthTests::loginFormAndAnimation()
{
    QTemporaryDir dir;
    DataStore store(std::make_unique<JsonRepository>(dir.filePath("app.json")));
    QVERIFY(store.initialize());
    LoginDialog dialog(&store);
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    auto *rotation = dialog.findChild<QVariantAnimation *>("loginRotation");
    QVERIFY(rotation);
    QCOMPARE(rotation->state(), QAbstractAnimation::Running);
    QTest::qWait(80);
    QVERIFY(rotation->currentValue().toReal() > 0);
    auto *username = dialog.findChild<QLineEdit *>("loginUsername");
    auto *password = dialog.findChild<QLineEdit *>("loginPassword");
    auto *confirmation = dialog.findChild<QLineEdit *>("loginConfirmation");
    auto *toggle = dialog.findChild<QPushButton *>("loginSwitch");
    auto *submit = dialog.findChild<QPushButton *>("loginSubmit");
    auto *error = dialog.findChild<QLabel *>("loginError");
    QCOMPARE(password->echoMode(), QLineEdit::Password);
    password->actions().first()->trigger();
    QCOMPARE(password->echoMode(), QLineEdit::Normal);
    QTest::mouseClick(toggle, Qt::LeftButton);
    QVERIFY(confirmation->isVisible());
    username->setText("demo");
    password->setText("password");
    confirmation->setText("wrong");
    QTest::mouseClick(submit, Qt::LeftButton);
    QVERIFY(!error->text().isEmpty());
    confirmation->setText("password");
    QTest::mouseClick(submit, Qt::LeftButton);
    QVERIFY(confirmation->isHidden());
    QCOMPARE(username->text(), QStringLiteral("demo"));
    QVERIFY(password->text().isEmpty());
    password->setText("password");
    QSignalSpy authenticated(&dialog, &LoginDialog::authenticated);
    QTest::keyClick(password, Qt::Key_Return);
    QCOMPARE(authenticated.count(), 1);
    QVERIFY(dialog.isVisible());
    QVERIFY(!submit->isEnabled());
    QCOMPARE(rotation->state(), QAbstractAnimation::Running);
    dialog.hide();
    QCOMPARE(rotation->state(), QAbstractAnimation::Stopped);
    QVERIFY(!store.isAdmin());
    // Optional artifacts for visual inspection; kept in build output only.
    if (!qEnvironmentVariableIsEmpty("QT_SYNC_AUTH_SCREENSHOT")) {
        LoginDialog preview(&store);
        preview.show();
        QTest::qWait(100);
        QVERIFY(preview.grab().save(qEnvironmentVariable("QT_SYNC_AUTH_SCREENSHOT")));
    }
}

void AuthTests::lastLoginPrefill()
{
    QTemporaryDir dir;
    DataStore store(std::make_unique<JsonRepository>(dir.filePath("app.json")));
    QVERIFY(store.initialize());
    LoginDialog dialog(&store);
    auto *username = dialog.findChild<QLineEdit *>("loginUsername");
    auto *password = dialog.findChild<QLineEdit *>("loginPassword");
    auto *submit = dialog.findChild<QPushButton *>("loginSubmit");
    QVERIFY(username->text().isEmpty());
    username->setText(" admin ");
    password->setText("wrong");
    submit->click();
    QVERIFY(!QFile::exists(dir.filePath("login-preferences.json")));
    password->setText("admin");
    submit->click();
    QVERIFY(QFile::exists(dir.filePath("login-preferences.json")));
    store.logout();
    dialog.resetForLogin();
    QCOMPARE(username->text(), QStringLiteral("admin"));
    QCOMPARE(password->text(), QStringLiteral("admin"));
    QVERIFY(store.currentUserId().isEmpty());

    // A fresh store/dialog restores the form without authenticating.
    DataStore restarted(std::make_unique<JsonRepository>(dir.filePath("app.json")));
    LoginDialog fresh(&restarted);
    QCOMPARE(fresh.findChild<QLineEdit *>("loginUsername")->text(), QStringLiteral("admin"));
    auto *restoredPassword = fresh.findChild<QLineEdit *>("loginPassword");
    QCOMPARE(restoredPassword->text(), QStringLiteral("admin"));
    QCOMPARE(restoredPassword->echoMode(), QLineEdit::Password);
    QVERIFY(restarted.currentUserId().isEmpty());
    fresh.findChild<QPushButton *>("loginSwitch")->click();
    QVERIFY(restoredPassword->text().isEmpty());
}

void AuthTests::loginQuitShortcut()
{
    QTemporaryDir dir;
    DataStore store(std::make_unique<JsonRepository>(dir.filePath("app.json")));
    LoginDialog dialog(&store);
    dialog.setBusy(true, QStringLiteral("正在准备…"));
    dialog.show();
    dialog.activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(&dialog));
    QSignalSpy rejected(&dialog, &QDialog::rejected);
    auto *quit = dialog.findChild<QAction *>("actionLoginQuit");
    QVERIFY(quit);
    QCOMPARE(quit->shortcut(), QKeySequence(QKeySequence::Quit));
    QTest::keySequence(&dialog, QKeySequence(QKeySequence::Quit));
    QTRY_COMPARE(rejected.count(), 1);
    QVERIFY(!dialog.isVisible());
    QVERIFY(store.initialize());
    dialog.resetForLogin();
    dialog.show();
    dialog.activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(&dialog));
    QTest::mouseClick(dialog.findChild<QPushButton *>("loginSwitch"), Qt::LeftButton);
    QTest::keySequence(dialog.findChild<QLineEdit *>("loginUsername"), QKeySequence(QKeySequence::Quit));
    QTRY_COMPARE(rejected.count(), 2);
}

void AuthTests::roleNavigationAndLogout()
{
    QTemporaryDir dir;
    // Temporary storage keeps background station refreshes away from real user data.
    DataStore store(std::make_unique<JsonRepository>(dir.filePath("app.json")));
    QVERIFY(store.initialize());
    QVERIFY(store.registerUser("user", "pass"));
    QVERIFY(store.login("user", "pass"));
    {
        MainWindow window(&store);
        for (const auto &name : {"btnAdmin", "btnStatistics"})
            QVERIFY(window.findChild<QToolButton *>(name)->isHidden());
        auto *stack = window.findChild<QStackedWidget *>("stackedWidget");
        QCOMPARE(stack->count(), 4);
        window.findChild<QToolButton *>("btnAdmin")->click();
        QCOMPARE(stack->currentWidget()->objectName(), QStringLiteral("pageQuery"));
        for (auto *action : window.findChildren<QAction *>()) {
            QVERIFY(action->shortcut() != QKeySequence("Ctrl+4"));
            QVERIFY(action->shortcut() != QKeySequence("Ctrl+5"));
            QVERIFY(!action->text().contains(QStringLiteral("打开数据目录")));
        }
        QVERIFY(window.findChild<QTableView *>("tableOrders")->isColumnHidden(OrderTableModel::OwnerColumn));
        window.show();
        QVERIFY(window.findChild<QToolButton *>("btnSettings")->isVisible());
        window.findChild<QToolButton *>("btnSettings")->click();
        QCOMPARE(stack->currentWidget()->objectName(), QStringLiteral("pageSettings"));
        QVERIFY(window.findChild<QWidget *>("settingsDataCard")->isHidden());
        QVERIFY(window.findChild<QWidget *>("settingsBackupCard")->isHidden());
        QVERIFY(window.findChild<QLabel *>("labelSettingsAccount")->text().contains("user"));
        QVERIFY(window.findChild<QPushButton *>("btnLogout")->isVisible());
        QSignalSpy closed(&window, &MainWindow::closed);
        window.findChild<QPushButton *>("btnLogout")->click();
        QVERIFY(window.logoutRequested());
        QCOMPARE(closed.count(), 1);
    }
    store.logout();
    QVERIFY(store.login("admin", "admin"));
    MainWindow admin(&store);
    QVERIFY(!admin.findChild<QWidget *>("settingsDataCard")->isHidden());
    QVERIFY(!admin.findChild<QWidget *>("settingsBackupCard")->isHidden());
    QCOMPARE(admin.findChild<QStackedWidget *>("stackedWidget")->count(), 6);
    QVERIFY(!admin.findChild<QTableView *>("tableOrders")->isColumnHidden(OrderTableModel::OwnerColumn));
    QVERIFY(!admin.findChild<QTableView *>("tablePassengers")->isColumnHidden(PassengerTableModel::OwnerColumn));
    const QDate today = QDate::currentDate();
    QCOMPARE(admin.findChild<QDateEdit *>("dateOrderFrom")->date(), QDate(today.year(), today.month(), 1));
    QCOMPARE(admin.findChild<QDateEdit *>("dateOrderTo")->date(), today);
}

void AuthTests::startupWithFullStationCatalog()
{
    QTemporaryDir dir;
    const QDir source(QFileInfo(QString::fromUtf8(__FILE__)).dir().filePath("../data"));
    QVERIFY(QFile::copy(source.filePath("railway-stations.json"), dir.filePath("railway-stations.json")));
    if (qEnvironmentVariableIsSet("QT_SYNC_PROFILE_STARTUP"))
        QVERIFY(QFile::copy(source.filePath("railway-cache.json"), dir.filePath("railway-cache.json")));
    DataStore store(std::make_unique<JsonRepository>(dir.filePath("app.json")));
    QVERIFY(store.initialize());
    QVERIFY(store.registerUser("profile", "pass"));
    QElapsedTimer timer;
    timer.start();
    QVERIFY(store.login("profile", "pass"));
    const qint64 loginMs = timer.restart();
    MainWindow window(&store);
    const qint64 constructMs = timer.restart();
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    const qint64 showMs = timer.restart();
    auto *departure = window.findChild<QComboBox *>("comboDepartureStation");
    auto *arrival = window.findChild<QComboBox *>("comboArrivalStation");
    QVERIFY(departure->count() >= 3000);
    QCOMPARE(departure->count(), arrival->count());
    QCOMPARE(departure->model(), arrival->model());
    QCOMPARE(departure->sizeAdjustPolicy(), QComboBox::AdjustToMinimumContentsLengthWithIcon);
    QSignalSpy resets(departure->model(), &QAbstractItemModel::modelReset);
    departure->setEditText(QStringLiteral("南京"));
    QVERIFY(store.commit(store.data()));
    const qint64 refreshMs = timer.elapsed();
    qInfo("Startup timings: login=%lld ms, construct=%lld ms, firstShow=%lld ms, unchangedData=%lld ms, stations=%d",
          loginMs, constructMs, showMs, refreshMs, departure->count());
    QCOMPARE(resets.count(), 0);
    QCOMPARE(departure->currentText(), QStringLiteral("南京"));
    const int originalCount = departure->count();
    const QString arrivalCode = arrival->currentData().toString();
    auto updated = store.data();
    updated.stations.append({"DEMO", QStringLiteral("南京"), QStringLiteral("南京"), true});
    QVERIFY(store.commit(updated));
    QCOMPARE(resets.count(), 1);
    QCOMPARE(departure->count(), originalCount + 1);
    QVERIFY(departure->findData("DEMO") >= 0);
    QVERIFY(departure->itemText(departure->findData("DEMO")).contains("DEMO"));
    QCOMPARE(arrival->currentData().toString(), arrivalCode);
    updated.stations.last().enabled = false;
    QVERIFY(store.commit(updated));
    QCOMPARE(resets.count(), 2);
    QCOMPARE(departure->count(), originalCount);
    QCOMPARE(departure->findData("DEMO"), -1);
}

void AuthTests::stationCatalogFreshness()
{
    QTemporaryDir dir;
    RailwayQueryService railway(dir.path());
    const auto now = QDateTime::currentDateTime();
    QVERIFY(railway.stationCatalogNeedsRefresh(now));
    QFile cache(dir.filePath("railway-cache.json"));
    const auto writeCache = [&](const QDateTime &fetchedAt) {
        if (!cache.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
        const QByteArray bytes = QJsonDocument(QJsonObject{
            {"stations", QJsonArray{QJsonObject{{"name", "Nanjing"}, {"code", "NJH"}}}},
            {"stationsFetchedAt", fetchedAt.toString(Qt::ISODate)}}).toJson();
        const bool written = cache.write(bytes) == bytes.size();
        cache.close();
        return written;
    };
    QVERIFY(writeCache(now.addSecs(-60)));
    QVERIFY(!railway.stationCatalogNeedsRefresh(now));
    QVERIFY(writeCache(now.addDays(-2)));
    QVERIFY(railway.stationCatalogNeedsRefresh(now));
    QVERIFY(writeCache(now.addDays(1)));
    QVERIFY(railway.stationCatalogNeedsRefresh(now));
}

QTEST_MAIN(AuthTests)
#include "auth_tests.moc"

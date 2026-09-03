#include "features/booking/bookingcontroller.h"

#include "data/datastore.h"
#include "models/trainqueryfilterproxymodel.h"
#include "models/trainquerymodel.h"
#include "services/bookingservice.h"
#include "services/passengerservice.h"
#include "services/railwayqueryservice.h"
#include "widgets/dialogstyle.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QEventLoop>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

namespace {
QString formatMoney(qint64 cents)
{
    return QStringLiteral("¥%1.%2")
        .arg(cents / 100)
        .arg(cents % 100, 2, 10, QLatin1Char('0'));
}
}

BookingController::BookingController(DataStore *dataStore,
                                     QTableView *table,
                                     QPushButton *bookButton,
                                     QObject *parent)
    : QObject(parent)
    , m_dataStore(dataStore)
    , m_table(table)
    , m_bookButton(bookButton)
    , m_railwayService(new RailwayQueryService(dataStore->dataDirectory(), this))
{
    connect(bookButton, &QPushButton::clicked, this, [this]() { bookSelected(); });
    connect(m_table, &QTableView::doubleClicked, this, [this](const QModelIndex &) { bookSelected(); });
}

void BookingController::bookSelected()
{
    const QModelIndex proxyIndex = m_table->currentIndex();
    if (!proxyIndex.isValid()) {
        QMessageBox::information(m_table, tr("购票"), tr("请先选择一条车次席别记录。"));
        return;
    }
    const auto *proxy = qobject_cast<const TrainQueryFilterProxyModel *>(m_table->model());
    const auto *model = proxy ? qobject_cast<const TrainQueryModel *>(proxy->sourceModel()) : nullptr;
    if (!proxy || !model)
        return;
    const QModelIndex sourceIndex = proxy->mapToSource(proxyIndex);
    const TrainQueryRow *row = model->rowAt(sourceIndex.row());
    const TrainSeatOption *seat = model->selectedSeatAt(sourceIndex.row());
    if (!row || !seat)
        return;
    if (seat->priceCents < 0) {
        QMessageBox::information(m_table, tr("购票"), tr("该席别没有可用票价，请切换其他席别。"));
        return;
    }

    QVector<DemoTrainSnapshot::RouteStop> routeStops;
    if (!row->bookable) {
        if (row->railwayTrainId.isEmpty()) {
            QMessageBox::warning(m_table,
                                 tr("无法安全购票"),
                                 tr("这条旧缓存没有 12306 内部车次标识，无法取得完整经停站并进行区间扣减。请重新在线查询，或使用自定义车次完成课程演示。"));
            return;
        }
        QVector<RailwayRouteStop> railwayStops;
        QString routeError;
        QEventLoop waitLoop;
        m_bookButton->setEnabled(false);
        m_bookButton->setText(tr("读取经停站…"));
        m_railwayService->queryRoute(*row, [&](QVector<RailwayRouteStop> stops, const QString &error) {
            railwayStops = std::move(stops);
            routeError = error;
            waitLoop.quit();
        });
        waitLoop.exec();
        m_bookButton->setText(tr("购票"));
        m_bookButton->setEnabled(true);
        if (!routeError.isEmpty() || railwayStops.size() < 2) {
            QMessageBox::warning(m_table,
                                 tr("无法安全购票"),
                                 routeError.isEmpty() ? tr("没有取得完整经停站，已取消本次演示购票以避免错误扣减。")
                                                      : routeError);
            return;
        }
        for (const RailwayRouteStop &stop : railwayStops)
            routeStops.append({stop.code, stop.name, stop.arrivalTime, stop.departureTime, stop.dayOffset});
    }

    QDialog dialog(m_table);
    dialog.setWindowTitle(tr("确认购票"));
    dialog.setMinimumWidth(500);
    applyStandardDialogStyle(&dialog);
    auto *layout = new QVBoxLayout(&dialog);
    auto *summary = new QLabel(
        tr("行程：%1 → %2\n车次：%3    日期：%4    出发：%5    历时：%6\n席别：%7    单价：%8    余票：%9")
            .arg(row->departureStationName,
                 row->arrivalStationName,
                 row->trainNumber,
                 row->serviceDate.toString(QStringLiteral("yyyy-MM-dd")),
                 row->departureTime.toString(QStringLiteral("HH:mm")),
                 QStringLiteral("%1小时%2分").arg(row->durationMinutes / 60).arg(row->durationMinutes % 60, 2, 10, QLatin1Char('0')),
                 seat->seatType,
                 formatMoney(seat->priceCents),
                 seat->availabilityText.isEmpty() ? QString::number(seat->remainingSeats)
                                                  : seat->availabilityText),
        &dialog);
    summary->setWordWrap(true);
    layout->addWidget(summary);
    layout->addWidget(new QLabel(tr("选择乘车人（可多选）"), &dialog));

    auto *passengers = new QListWidget(&dialog);
    passengers->setSelectionMode(QAbstractItemView::NoSelection);
    for (const domain::Passenger &passenger : m_dataStore->data().passengers) {
        auto *item = new QListWidgetItem(
            tr("%1  ·  %2").arg(passenger.name, PassengerService::maskedDocumentNumber(passenger.documentNumber)),
            passengers);
        item->setData(Qt::UserRole, passenger.id);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
    }
    layout->addWidget(passengers);
    auto *totalLabel = new QLabel(tr("合计：¥0.00"), &dialog);
    totalLabel->setAlignment(Qt::AlignRight);
    layout->addWidget(totalLabel);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, &dialog);
    QPushButton *confirmButton = buttons->addButton(tr("生成演示订单"), QDialogButtonBox::AcceptRole);
    confirmButton->setDefault(true);
    confirmButton->setEnabled(false);
    layout->addWidget(buttons);

    auto *disclaimer = new QLabel(
        tr("数据来自中国铁路12306；本页面购票仅用于课程演示，不能用于真实购票。"),
        &dialog);
    disclaimer->setWordWrap(true);
    disclaimer->setAlignment(Qt::AlignCenter);
    disclaimer->setStyleSheet(QStringLiteral("color: #8E8E93; font-size: 12px; padding: 6px 0 2px 0;"));
    layout->addWidget(disclaimer);

    connect(passengers, &QListWidget::itemChanged, &dialog, [=]() {
        int selected = 0;
        for (int i = 0; i < passengers->count(); ++i)
            selected += passengers->item(i)->checkState() == Qt::Checked;
        confirmButton->setEnabled(selected > 0);
        totalLabel->setText(tr("合计：%1").arg(formatMoney(seat->priceCents * selected)));
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    BookingReceipt receipt;
    bool bookingSucceeded = false;
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        QStringList passengerIds;
        for (int i = 0; i < passengers->count(); ++i) {
            if (passengers->item(i)->checkState() == Qt::Checked)
                passengerIds.push_back(passengers->item(i)->data(Qt::UserRole).toString());
        }
        const BookingRequest request{row->trainNumber,
                                     row->serviceDate,
                                     row->departureStationCode,
                                     row->arrivalStationCode,
                                     seat->seatType,
                                     passengerIds,
                                     row->railwayTrainId};
        BookingService service(m_dataStore);
        const OperationResult result = row->bookable
            ? service.book(request, &receipt)
            : service.bookDemo(request,
                               {row->departureStationName,
                                row->arrivalStationName,
                                row->departureTime,
                                row->arrivalTime,
                                row->departureDayOffset,
                                row->arrivalDayOffset,
                                seat->priceCents,
                                seat->availabilityText == QStringLiteral("有")
                                    ? 50 : seat->remainingSeats,
                                routeStops},
                               &receipt);
        if (!result) {
            QMessageBox::warning(&dialog, tr("购票失败"), result.error);
            return;
        }
        bookingSucceeded = true;
        dialog.accept();
    });
    dialog.exec();
    if (bookingSucceeded) {
        QMessageBox::information(m_table,
                                 tr("购票成功"),
                                 tr("本地演示订单已生成：%1\n共出票 %2 张，合计 %3。")
                                     .arg(receipt.orderId)
                                     .arg(receipt.ticketIds.size())
                                     .arg(formatMoney(receipt.totalAmountCents)));
    }
}

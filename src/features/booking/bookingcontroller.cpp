#include "features/booking/bookingcontroller.h"
#include "common/money.h"

#include "data/datastore.h"
#include "features/query/trainqueryfilterproxymodel.h"
#include "features/query/trainquerymodel.h"
#include "features/booking/bookingservice.h"
#include "features/passengers/passengerservice.h"
#include "features/query/railwayqueryservice.h"
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

BookingController::BookingController(DataStore *dataStore,
                                     QTableView *table,
                                     QPushButton *bookButton,
                                     QObject *parent)
    : QObject(parent)
    , m_dataStore(dataStore)
    , m_table(table)
    , m_proxy(qobject_cast<TrainQueryFilterProxyModel *>(table->model()))
    , m_model(m_proxy ? qobject_cast<TrainQueryModel *>(m_proxy->sourceModel()) : nullptr)
    , m_bookButton(bookButton)
    , m_railwayService(new RailwayQueryService(dataStore->dataDirectory(), this))
{
    // QueryController 会在创建此控制器前安装这些模型。
    Q_ASSERT(m_proxy && m_model);
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
    const QModelIndex sourceIndex = m_proxy->mapToSource(proxyIndex);
    const TrainQueryRow *row = m_model->rowAt(sourceIndex.row());
    const TrainSeatOption *seat = m_model->selectedSeatAt(sourceIndex.row());
    if (!row || !seat)
        return;
    if (!row->bookable) {
        QMessageBox::information(m_table,
                                 tr("外部数据只读"),
                                 tr("12306 是可选外部查询源，不参与本地座位分配。请取消勾选“使用 12306 数据（外部）”，从本地车次完成演示购票。"));
        return;
    }
    if (seat->priceCents < 0) {
        QMessageBox::information(m_table, tr("购票"), tr("该席别没有可用票价，请切换其他席别。"));
        return;
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
                 common::formatMoney(seat->priceCents),
                 seat->availabilityText.isEmpty() ? QString::number(seat->remainingSeats)
                                                  : seat->availabilityText),
        &dialog);
    summary->setWordWrap(true);
    layout->addWidget(summary);
    layout->addWidget(new QLabel(tr("选择乘车人（可多选）"), &dialog));

    auto *passengers = new QListWidget(&dialog);
    passengers->setSelectionMode(QAbstractItemView::NoSelection);
    for (const domain::Passenger &passenger : m_dataStore->data().passengers) {
        if (!m_dataStore->canAccessOwner(passenger.ownerUserId))
            continue;
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

    auto *disclaimer = new QLabel(tr("当前使用演示数据。"), &dialog);
    disclaimer->setWordWrap(true);
    disclaimer->setAlignment(Qt::AlignCenter);
    disclaimer->setStyleSheet(QStringLiteral("color: #8E8E93; font-size: 12px; padding: 6px 0 2px 0;"));
    layout->addWidget(disclaimer);

    connect(passengers, &QListWidget::itemChanged, &dialog, [=]() {
        int selected = 0;
        for (int i = 0; i < passengers->count(); ++i)
            selected += passengers->item(i)->checkState() == Qt::Checked;
        confirmButton->setEnabled(selected > 0);
        totalLabel->setText(tr("合计：%1").arg(common::formatMoney(seat->priceCents * selected)));
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
        BookingService service(*m_dataStore);
        const OperationResult result = service.book(request, &receipt);
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
                                 tr("本地演示订单已生成：%1\n座位：%2\n共出票 %3 张，合计 %4。")
                                     .arg(receipt.orderId)
                                     .arg(receipt.seatIds.join(QStringLiteral("、")))
                                     .arg(receipt.ticketIds.size())
                                     .arg(common::formatMoney(receipt.totalAmountCents)));
    }
}

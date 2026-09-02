#include "features/booking/bookingcontroller.h"

#include "data/datastore.h"
#include "models/trainqueryfilterproxymodel.h"
#include "models/trainquerymodel.h"
#include "services/bookingservice.h"
#include "services/passengerservice.h"
#include "widgets/dialogstyle.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
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
                                     QComboBox *departureStation,
                                     QComboBox *arrivalStation,
                                     QDateEdit *travelDate,
                                     QPushButton *bookButton,
                                     QObject *parent)
    : QObject(parent)
    , m_dataStore(dataStore)
    , m_table(table)
    , m_departureStation(departureStation)
    , m_arrivalStation(arrivalStation)
    , m_travelDate(travelDate)
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

    QDialog dialog(m_table);
    dialog.setWindowTitle(tr("确认购票"));
    dialog.setMinimumWidth(500);
    applyStandardDialogStyle(&dialog);
    auto *layout = new QVBoxLayout(&dialog);
    auto *summary = new QLabel(
        tr("行程：%1 → %2\n车次：%3    出发：%4    历时：%5\n席别：%6    单价：%7    余票：%8")
            .arg(row->departureStationName,
                 row->arrivalStationName,
                 row->trainNumber,
                 row->departureTime.toString(QStringLiteral("HH:mm")),
                 QStringLiteral("%1小时%2分").arg(row->durationMinutes / 60).arg(row->durationMinutes % 60, 2, 10, QLatin1Char('0')),
                 seat->seatType,
                 formatMoney(seat->priceCents),
                 QString::number(seat->remainingSeats)),
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
    QPushButton *confirmButton = buttons->addButton(tr("确认支付并出票"), QDialogButtonBox::AcceptRole);
    confirmButton->setDefault(true);
    confirmButton->setEnabled(false);
    layout->addWidget(buttons);

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
        const OperationResult result = BookingService(m_dataStore).book(
            {row->trainNumber,
             m_travelDate->date(),
             m_departureStation->currentData().toString(),
             m_arrivalStation->currentData().toString(),
             seat->seatType,
             passengerIds},
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
                                 tr("订单已生成：%1\n共出票 %2 张，合计 %3。")
                                     .arg(receipt.orderId)
                                     .arg(receipt.ticketIds.size())
                                     .arg(formatMoney(receipt.totalAmountCents)));
    }
}

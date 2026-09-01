#include "features/orders/ordercontroller.h"

#include "data/datastore.h"
#include "models/orderfilterproxymodel.h"
#include "models/orderdelegates.h"
#include "models/ordertablemodel.h"
#include "services/orderservice.h"
#include "services/refundservice.h"
#include "widgets/dialogstyle.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardItemModel>
#include <QTableView>
#include <QVBoxLayout>

namespace {
QString formatMoney(qint64 cents)
{
    return QStringLiteral("¥%1.%2")
        .arg(cents / 100)
        .arg(cents % 100, 2, 10, QLatin1Char('0'));
}

QStandardItem *detailItem(const QString &text, const QString &ticketId = {})
{
    auto *item = new QStandardItem(text);
    item->setEditable(false);
    item->setTextAlignment(Qt::AlignCenter);
    if (!ticketId.isEmpty())
        item->setData(ticketId, Qt::UserRole);
    return item;
}
} // namespace

OrderController::OrderController(DataStore *dataStore,
                                 QWidget *dialogParent,
                                 QTableView *table,
                                 QLineEdit *searchEdit,
                                 QComboBox *statusCombo,
                                 QDateEdit *dateFrom,
                                 QDateEdit *dateTo,
                                 QPushButton *searchButton,
                                 QPushButton *detailsButton,
                                 QPushButton *refundButton,
                                 QObject *parent)
    : QObject(parent)
    , m_dataStore(dataStore)
    , m_dialogParent(dialogParent)
    , m_table(table)
    , m_searchEdit(searchEdit)
    , m_statusCombo(statusCombo)
    , m_dateFrom(dateFrom)
    , m_dateTo(dateTo)
    , m_detailsButton(detailsButton)
    , m_refundButton(refundButton)
    , m_model(new OrderTableModel(dataStore, this))
    , m_proxy(new OrderFilterProxyModel(this))
{
    m_statusCombo->clear();
    m_statusCombo->addItem(tr("全部状态"), -1);
    m_statusCombo->addItem(tr("已出票"), static_cast<int>(domain::TicketStatus::Issued));
    m_statusCombo->addItem(tr("已退票"), static_cast<int>(domain::TicketStatus::Refunded));
    m_statusCombo->addItem(tr("已完成"), static_cast<int>(domain::TicketStatus::Completed));

    m_proxy->setSourceModel(m_model);
    m_table->setModel(m_proxy);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSortingEnabled(true);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setItemDelegateForColumn(OrderTableModel::PriceColumn,
                                      new OrderMoneyDelegate(m_table));
    m_table->setItemDelegateForColumn(OrderTableModel::StatusColumn,
                                      new OrderStatusDelegate(m_table));

    connect(searchButton, &QPushButton::clicked, this, [this]() { applyFilters(true); });
    connect(m_searchEdit, &QLineEdit::returnPressed, this, [this]() { applyFilters(true); });
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this]() { applyFilters(false); });
    connect(m_statusCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this]() { applyFilters(false); });
    connect(m_dateFrom, &QDateEdit::dateChanged, this, [this]() { applyFilters(false); });
    connect(m_dateTo, &QDateEdit::dateChanged, this, [this]() { applyFilters(false); });
    connect(m_detailsButton, &QPushButton::clicked, this, [this]() { openSelectedOrder(false); });
    connect(m_refundButton, &QPushButton::clicked, this, [this]() { openSelectedOrder(true); });
    connect(m_table, &QTableView::doubleClicked, this, [this](const QModelIndex &) {
        openSelectedOrder(false);
    });
    connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this]() { updateActionButtons(); });
    connect(m_model, &QAbstractItemModel::modelReset, this, [this]() { updateActionButtons(); });

    const OperationResult refreshed = OrderService(m_dataStore).refreshCompletedTickets();
    Q_UNUSED(refreshed)
    applyFilters(false);
    updateActionButtons();
}

void OrderController::applyFilters(bool refreshStatuses)
{
    if (m_dateFrom->date() > m_dateTo->date()) {
        if (refreshStatuses)
            QMessageBox::warning(m_dialogParent, tr("订单筛选"), tr("开始日期不能晚于结束日期。"));
        m_proxy->setDateRange(m_dateFrom->date(), m_dateTo->date());
        updateActionButtons();
        return;
    }
    if (refreshStatuses) {
        const OperationResult result = OrderService(m_dataStore).refreshCompletedTickets();
        if (!result) {
            QMessageBox::warning(m_dialogParent, tr("无法刷新订单"), result.error);
            return;
        }
    }
    m_proxy->setKeyword(m_searchEdit->text());
    m_proxy->setStatus(m_statusCombo->currentData().toInt());
    m_proxy->setDateRange(m_dateFrom->date(), m_dateTo->date());
    if (m_proxy->rowCount() > 0)
        m_table->selectRow(0);
    else
        m_table->clearSelection();
    updateActionButtons();
}

QString OrderController::selectedOrderId() const
{
    const QModelIndex proxyIndex = m_table->currentIndex();
    if (!proxyIndex.isValid())
        return {};
    return m_model->data(m_proxy->mapToSource(proxyIndex), Qt::UserRole).toString();
}

void OrderController::openSelectedOrder(bool refundMode)
{
    const QString orderId = selectedOrderId();
    if (orderId.isEmpty()) {
        QMessageBox::information(m_dialogParent, tr("订单"), tr("请先选择一条订单。"));
        return;
    }
    const QString ticketId = showOrderDetails(orderId, refundMode);
    if (!ticketId.isEmpty())
        confirmRefund(ticketId);
}

QString OrderController::showOrderDetails(const QString &orderId, bool refundMode)
{
    const QVector<OrderTicketDetail> details = OrderService(m_dataStore).details(orderId);
    if (details.isEmpty()) {
        QMessageBox::warning(m_dialogParent, tr("订单详情"), tr("订单中没有可显示的车票。"));
        return {};
    }

    QDialog dialog(m_dialogParent);
    dialog.setWindowTitle(refundMode ? tr("选择退票车票") : tr("订单详情"));
    dialog.resize(900, 420);
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(12);
    layout->addWidget(new QLabel(tr("订单号：%1").arg(orderId), &dialog));

    auto *table = new QTableView(&dialog);
    auto *model = new QStandardItemModel(0, 7, table);
    model->setHorizontalHeaderLabels({tr("票号"), tr("乘车人"), tr("行程"), tr("车次/日期"),
                                      tr("席别"), tr("票价"), tr("状态")});
    for (const OrderTicketDetail &ticket : details) {
        QList<QStandardItem *> items;
        items << detailItem(ticket.ticketId, ticket.ticketId)
              << detailItem(ticket.passengerName)
              << detailItem(tr("%1 → %2").arg(ticket.departureStationName, ticket.arrivalStationName))
              << detailItem(tr("%1\n%2 %3")
                                .arg(ticket.trainNumber,
                                     ticket.serviceDate.toString(QStringLiteral("yyyy-MM-dd")),
                                     ticket.departureAt.time().toString(QStringLiteral("HH:mm"))))
              << detailItem(ticket.seatType)
              << detailItem(formatMoney(ticket.priceCents))
              << detailItem(OrderService::statusText(ticket.status));
        model->appendRow(items);
    }
    table->setModel(model);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);
    table->resizeRowsToContents();
    layout->addWidget(table);

    auto *hint = new QLabel(refundMode ? tr("请选择一张未出行的有效车票。")
                                       : tr("选择具体车票后可以办理退票。"),
                            &dialog);
    layout->addWidget(hint);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    auto *selectRefund = buttons->addButton(tr("选择此票退票"), QDialogButtonBox::ActionRole);
    selectRefund->setEnabled(false);
    buttons->button(QDialogButtonBox::Close)->setText(tr("关闭"));
    layout->addWidget(buttons);
    applyStandardDialogStyle(&dialog);

    QString requestedTicketId;
    const auto updateSelection = [=]() {
        const QModelIndex index = table->currentIndex();
        if (!index.isValid()) {
            selectRefund->setEnabled(false);
            return;
        }
        const QString ticketId = model->index(index.row(), 0).data(Qt::UserRole).toString();
        RefundQuote quote;
        const OperationResult result = RefundService(m_dataStore).quote(
            ticketId, QDateTime::currentDateTime(), &quote);
        selectRefund->setEnabled(result.success);
        hint->setText(result ? tr("可退金额：%1（手续费 %2%，%3）")
                                   .arg(formatMoney(quote.refundAmountCents))
                                   .arg(quote.ratePercent)
                                   .arg(formatMoney(quote.feeCents))
                             : result.error);
    };
    connect(table->selectionModel(), &QItemSelectionModel::selectionChanged,
            &dialog, [=]() { updateSelection(); });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(selectRefund, &QPushButton::clicked, &dialog, [&]() {
        const QModelIndex index = table->currentIndex();
        if (!index.isValid())
            return;
        requestedTicketId = model->index(index.row(), 0).data(Qt::UserRole).toString();
        dialog.accept();
    });
    if (model->rowCount() > 0) {
        table->selectRow(0);
        updateSelection();
    }
    dialog.exec();
    return requestedTicketId;
}

void OrderController::confirmRefund(const QString &ticketId)
{
    RefundService service(m_dataStore);
    RefundQuote quote;
    OperationResult result = service.quote(ticketId, QDateTime::currentDateTime(), &quote);
    if (!result) {
        QMessageBox::warning(m_dialogParent, tr("无法退票"), result.error);
        return;
    }
    const QString message = tr("确定退票吗？\n\n手续费率：%1%\n手续费：%2\n退款金额：%3")
                                .arg(quote.ratePercent)
                                .arg(formatMoney(quote.feeCents))
                                .arg(formatMoney(quote.refundAmountCents));
    if (QMessageBox::question(m_dialogParent,
                              tr("确认退票"),
                              message,
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No)
        != QMessageBox::Yes) {
        return;
    }
    RefundReceipt receipt;
    result = service.refund(ticketId, QDateTime::currentDateTime(), &receipt);
    if (!result) {
        QMessageBox::warning(m_dialogParent, tr("退票失败"), result.error);
        return;
    }
    QMessageBox::information(m_dialogParent,
                             tr("退票成功"),
                             tr("退款 %1，手续费 %2。")
                                 .arg(formatMoney(receipt.refundAmountCents),
                                      formatMoney(receipt.feeCents)));
}

void OrderController::updateActionButtons()
{
    const bool selected = !selectedOrderId().isEmpty();
    m_detailsButton->setEnabled(selected);
    m_refundButton->setEnabled(selected);
}

#include "models/ordertablemodel.h"

#include "data/datastore.h"

#include <QStringList>

OrderTableModel::OrderTableModel(DataStore *dataStore, QObject *parent)
    : QAbstractTableModel(parent)
    , m_dataStore(dataStore)
{
    connect(m_dataStore, &DataStore::dataChanged, this, &OrderTableModel::reload);
    reload();
}

int OrderTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

int OrderTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant OrderTableModel::data(const QModelIndex &index, int role) const
{
    const OrderSummary *row = rowAt(index.row());
    if (!index.isValid() || !row)
        return {};
    if (role == Qt::UserRole)
        return row->orderId;
    if (role == Qt::UserRole + 1)
        return static_cast<int>(row->status);
    if (role == Qt::UserRole + 2)
        return row->serviceDate;
    if (role == Qt::UserRole + 3)
        return row->totalAmountCents;
    if (role == Qt::TextAlignmentRole)
        return Qt::AlignCenter;
    if (role != Qt::DisplayRole)
        return {};

    switch (index.column()) {
    case OrderNumberColumn:
        return row->orderId;
    case PassengerColumn:
        return row->passengerNames.join(QStringLiteral("、"));
    case TrainColumn:
        return row->trainNumbers.join(QStringLiteral("、"));
    case DepartureDateColumn:
        return row->serviceDate.toString(QStringLiteral("yyyy-MM-dd"));
    case SeatTypeColumn:
        return row->seatTypes.join(QStringLiteral("、"));
    case PriceColumn:
        return row->totalAmountCents;
    case StatusColumn:
        return static_cast<int>(row->status);
    default:
        return {};
    }
}

QVariant OrderTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QAbstractTableModel::headerData(section, orientation, role);
    static const QStringList headers = {QStringLiteral("订单号"),
                                        QStringLiteral("乘车人"),
                                        QStringLiteral("车次"),
                                        QStringLiteral("出发日期"),
                                        QStringLiteral("席别"),
                                        QStringLiteral("票价"),
                                        QStringLiteral("状态")};
    return section >= 0 && section < headers.size() ? headers.at(section) : QVariant{};
}

const OrderSummary *OrderTableModel::rowAt(int row) const
{
    return row >= 0 && row < m_rows.size() ? &m_rows.at(row) : nullptr;
}

QString OrderTableModel::searchableText(int row) const
{
    const OrderSummary *summary = rowAt(row);
    return summary ? summary->orderId + QLatin1Char('\n')
                         + summary->passengerNames.join(QLatin1Char('\n'))
                         + QLatin1Char('\n') + summary->trainNumbers.join(QLatin1Char('\n'))
                   : QString();
}

void OrderTableModel::reload()
{
    beginResetModel();
    m_rows = OrderService(m_dataStore).summaries();
    endResetModel();
}

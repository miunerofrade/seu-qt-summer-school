#include "models/orderfilterproxymodel.h"

#include "models/ordertablemodel.h"

OrderFilterProxyModel::OrderFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    setSortCaseSensitivity(Qt::CaseInsensitive);
}

void OrderFilterProxyModel::setKeyword(const QString &keyword)
{
    const QString normalized = keyword.trimmed();
    if (m_keyword == normalized)
        return;
    m_keyword = normalized;
    invalidateRows();
}

void OrderFilterProxyModel::setStatus(int status)
{
    if (m_status == status)
        return;
    m_status = status;
    invalidateRows();
}

void OrderFilterProxyModel::setDateRange(const QDate &from, const QDate &to)
{
    if (m_from == from && m_to == to)
        return;
    m_from = from;
    m_to = to;
    invalidateRows();
}

bool OrderFilterProxyModel::filterAcceptsRow(int sourceRow,
                                             const QModelIndex &sourceParent) const
{
    Q_UNUSED(sourceParent)
    const auto *model = qobject_cast<const OrderTableModel *>(sourceModel());
    const OrderSummary *row = model ? model->rowAt(sourceRow) : nullptr;
    if (!row)
        return false;
    if (!m_keyword.isEmpty()
        && !model->searchableText(sourceRow).contains(m_keyword, Qt::CaseInsensitive)) {
        return false;
    }
    if (m_status >= 0 && static_cast<int>(row->status) != m_status)
        return false;
    if (m_from.isValid() && row->serviceDate.isValid() && row->serviceDate < m_from)
        return false;
    if (m_to.isValid() && row->serviceDate.isValid() && row->serviceDate > m_to)
        return false;
    return true;
}

void OrderFilterProxyModel::invalidateRows()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
    endFilterChange(Direction::Rows);
#else
    invalidateFilter();
#endif
}

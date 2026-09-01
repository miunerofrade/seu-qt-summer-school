#include "models/passengerfilterproxymodel.h"

#include "models/passengertablemodel.h"

PassengerFilterProxyModel::PassengerFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    setSortCaseSensitivity(Qt::CaseInsensitive);
}

void PassengerFilterProxyModel::setKeyword(const QString &keyword)
{
    const QString normalized = keyword.trimmed();
    if (m_keyword == normalized)
        return;
    m_keyword = normalized;
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
    endFilterChange(Direction::Rows);
#else
    invalidateFilter();
#endif
}

bool PassengerFilterProxyModel::filterAcceptsRow(int sourceRow,
                                                const QModelIndex &sourceParent) const
{
    Q_UNUSED(sourceParent)
    if (m_keyword.isEmpty())
        return true;
    const auto *model = qobject_cast<const PassengerTableModel *>(sourceModel());
    return model && model->searchableText(sourceRow).contains(m_keyword, Qt::CaseInsensitive);
}

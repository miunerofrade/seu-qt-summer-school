#include "models/trainqueryfilterproxymodel.h"

#include "models/trainquerymodel.h"

#include <algorithm>

TrainQueryFilterProxyModel::TrainQueryFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    setSortCaseSensitivity(Qt::CaseInsensitive);
}

void TrainQueryFilterProxyModel::setTrainNumberFilter(const QString &number)
{
    const QString normalized = number.trimmed();
    if (m_trainNumber == normalized)
        return;
    m_trainNumber = normalized;
    refreshFilter();
}

void TrainQueryFilterProxyModel::setSeatTypeFilter(const QString &seatType)
{
    const QString normalized = seatType.trimmed();
    if (m_seatType == normalized)
        return;
    m_seatType = normalized;
    refreshFilter();
}

void TrainQueryFilterProxyModel::setAvailableOnly(bool availableOnly)
{
    if (m_availableOnly == availableOnly)
        return;
    m_availableOnly = availableOnly;
    refreshFilter();
}

void TrainQueryFilterProxyModel::setSortByPrice(bool sortByPrice)
{
    if (m_sortByPrice == sortByPrice)
        return;
    m_sortByPrice = sortByPrice;
    invalidate();
    sort(0, Qt::AscendingOrder);
}

bool TrainQueryFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    Q_UNUSED(sourceParent)
    const auto *model = qobject_cast<const TrainQueryModel *>(sourceModel());
    if (!model)
        return false;
    const TrainQueryRow *row = model->rowAt(sourceRow);
    if (!row)
        return false;
    const bool hasMatchingSeat = std::any_of(row->seats.cbegin(), row->seats.cend(), [this](const TrainSeatOption &seat) {
        return (m_seatType.isEmpty() || seat.seatType == m_seatType)
            && (!m_availableOnly || seat.remainingSeats > 0);
    });
    return (m_trainNumber.isEmpty() || row->trainNumber == m_trainNumber) && hasMatchingSeat;
}

bool TrainQueryFilterProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    const auto *model = qobject_cast<const TrainQueryModel *>(sourceModel());
    if (!model)
        return QSortFilterProxyModel::lessThan(left, right);
    const TrainQueryRow *leftRow = model->rowAt(left.row());
    const TrainQueryRow *rightRow = model->rowAt(right.row());
    if (!leftRow || !rightRow)
        return false;
    if (m_sortByPrice) {
        const TrainSeatOption *leftSeat = model->selectedSeatAt(left.row());
        const TrainSeatOption *rightSeat = model->selectedSeatAt(right.row());
        return leftSeat && rightSeat ? leftSeat->priceCents < rightSeat->priceCents : leftSeat != nullptr;
    }
    const int leftTime = leftRow->departureTime.hour() * 60 + leftRow->departureTime.minute()
                         + leftRow->departureDayOffset * 24 * 60;
    const int rightTime = rightRow->departureTime.hour() * 60 + rightRow->departureTime.minute()
                          + rightRow->departureDayOffset * 24 * 60;
    if (leftTime != rightTime)
        return leftTime < rightTime;
    return leftRow->trainNumber < rightRow->trainNumber;
}

void TrainQueryFilterProxyModel::refreshFilter()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
    endFilterChange(Direction::Rows);
#else
    invalidateFilter();
#endif
}

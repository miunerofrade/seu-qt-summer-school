#include "models/trainquerymodel.h"

#include <QStringList>

namespace {
QString formatMoney(qint64 cents)
{
    return QStringLiteral("¥%1.%2")
        .arg(cents / 100)
        .arg(cents % 100, 2, 10, QLatin1Char('0'));
}

QString formatDuration(int minutes)
{
    return QStringLiteral("%1小时%2分").arg(minutes / 60).arg(minutes % 60, 2, 10, QLatin1Char('0'));
}
} // namespace

TrainQueryModel::TrainQueryModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int TrainQueryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

int TrainQueryModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant TrainQueryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const TrainQueryRow &row = m_rows.at(index.row());
    if (role == Qt::UserRole)
        return row.trainNumber;
    if (role == Qt::UserRole + 1)
        return row.seatType;
    if (role == Qt::UserRole + 2)
        return row.remainingSeats;
    if (role == Qt::UserRole + 3)
        return row.priceCents;
    if (role == Qt::UserRole + 4)
        return row.departureTime;
    if (role == Qt::TextAlignmentRole)
        return Qt::AlignCenter;
    if (role != Qt::DisplayRole)
        return {};

    switch (index.column()) {
    case TrainNumberColumn:
        return row.trainNumber;
    case DepartureStationColumn:
        return row.departureStationName;
    case ArrivalStationColumn:
        return row.arrivalStationName;
    case DepartureTimeColumn:
        return row.departureTime.toString(QStringLiteral("HH:mm"));
    case DurationColumn:
        return formatDuration(row.durationMinutes);
    case SeatTypeColumn:
        return row.seatType;
    case RemainingSeatsColumn:
        return row.remainingSeats;
    case PriceColumn:
        return formatMoney(row.priceCents);
    default:
        return {};
    }
}

QVariant TrainQueryModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QAbstractTableModel::headerData(section, orientation, role);
    static const QStringList headers = {
        QStringLiteral("车次"), QStringLiteral("出发站"), QStringLiteral("到达站"),
        QStringLiteral("出发时间"), QStringLiteral("历时"), QStringLiteral("席别"),
        QStringLiteral("余票"), QStringLiteral("票价")};
    return section >= 0 && section < headers.size() ? headers.at(section) : QVariant{};
}

void TrainQueryModel::setRows(QVector<TrainQueryRow> rows)
{
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
}

const TrainQueryRow *TrainQueryModel::rowAt(int row) const
{
    return row >= 0 && row < m_rows.size() ? &m_rows.at(row) : nullptr;
}

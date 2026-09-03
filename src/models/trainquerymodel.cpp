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
    const TrainSeatOption *seat = selectedSeatAt(index.row());
    if (role == TrainNumberRole)
        return row.trainNumber;
    if (role == SeatTypeRole)
        return seat ? seat->seatType : QString();
    if (role == RemainingSeatsRole)
        return seat ? seat->remainingSeats : 0;
    if (role == PriceRole)
        return seat ? seat->priceCents : 0;
    if (role == DepartureTimeRole)
        return row.departureTime;
    if (role == SeatOptionsRole) {
        QStringList options;
        options.reserve(row.seats.size());
        for (const TrainSeatOption &option : row.seats)
            options.append(option.seatType);
        return options;
    }
    if (role == Qt::TextAlignmentRole)
        return Qt::AlignCenter;
    if (role != Qt::DisplayRole)
        return {};

    switch (index.column()) {
    case TrainNumberColumn:
        return row.trainNumber;
    case ServiceDateColumn:
        return row.serviceDate.toString(QStringLiteral("yyyy-MM-dd"));
    case DepartureStationColumn:
        return row.departureStationName;
    case ArrivalStationColumn:
        return row.arrivalStationName;
    case DepartureTimeColumn:
        return row.departureTime.toString(QStringLiteral("HH:mm"));
    case DurationColumn:
        return formatDuration(row.durationMinutes);
    case SeatTypeColumn:
        return seat ? seat->seatType : QString();
    case RemainingSeatsColumn:
        return seat ? seat->remainingSeats : 0;
    case PriceColumn:
        return seat ? formatMoney(seat->priceCents) : QString();
    default:
        return {};
    }
}

bool TrainQueryModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.column() != SeatTypeColumn || role != Qt::EditRole
        || index.row() < 0 || index.row() >= m_rows.size())
        return false;
    const auto &seats = m_rows.at(index.row()).seats;
    const QString requested = value.toString();
    for (int seatIndex = 0; seatIndex < seats.size(); ++seatIndex) {
        if (seats.at(seatIndex).seatType != requested)
            continue;
        if (m_selectedSeats.at(index.row()) == seatIndex)
            return true;
        m_selectedSeats[index.row()] = seatIndex;
        emit dataChanged(this->index(index.row(), SeatTypeColumn),
                         this->index(index.row(), PriceColumn),
                         {Qt::DisplayRole, SeatTypeRole, RemainingSeatsRole, PriceRole});
        return true;
    }
    return false;
}

Qt::ItemFlags TrainQueryModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags result = QAbstractTableModel::flags(index);
    if (index.isValid() && index.column() == SeatTypeColumn)
        result |= Qt::ItemIsEditable;
    return result;
}

QVariant TrainQueryModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QAbstractTableModel::headerData(section, orientation, role);
    static const QStringList headers = {
        QStringLiteral("车次"), QStringLiteral("日期"), QStringLiteral("出发站"), QStringLiteral("到达站"),
        QStringLiteral("出发时间"), QStringLiteral("历时"), QStringLiteral("席别"),
        QStringLiteral("余票"), QStringLiteral("票价")};
    return section >= 0 && section < headers.size() ? headers.at(section) : QVariant{};
}

void TrainQueryModel::setRows(QVector<TrainQueryRow> rows)
{
    beginResetModel();
    m_rows = std::move(rows);
    m_selectedSeats.fill(0, m_rows.size());
    endResetModel();
}

const TrainQueryRow *TrainQueryModel::rowAt(int row) const
{
    return row >= 0 && row < m_rows.size() ? &m_rows.at(row) : nullptr;
}

const TrainSeatOption *TrainQueryModel::selectedSeatAt(int row) const
{
    const TrainQueryRow *queryRow = rowAt(row);
    if (!queryRow || row >= m_selectedSeats.size())
        return nullptr;
    const int seatIndex = m_selectedSeats.at(row);
    return seatIndex >= 0 && seatIndex < queryRow->seats.size() ? &queryRow->seats.at(seatIndex) : nullptr;
}

void TrainQueryModel::selectSeatType(const QString &seatType, bool availableOnly)
{
    for (int row = 0; row < m_rows.size(); ++row) {
        const auto &seats = m_rows.at(row).seats;
        int selected = -1;
        for (int seatIndex = 0; seatIndex < seats.size(); ++seatIndex) {
            if ((!seatType.isEmpty() && seats.at(seatIndex).seatType == seatType)
                || (seatType.isEmpty() && (!availableOnly || seats.at(seatIndex).remainingSeats > 0))) {
                selected = seatIndex;
                break;
            }
        }
        if (selected < 0 || selected == m_selectedSeats.at(row))
            continue;
        m_selectedSeats[row] = selected;
        emit dataChanged(index(row, SeatTypeColumn), index(row, PriceColumn),
                         {Qt::DisplayRole, SeatTypeRole, RemainingSeatsRole, PriceRole});
    }
}

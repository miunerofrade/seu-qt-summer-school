#include "models/statisticsmodel.h"

#include <QStringList>

StatisticsModel::StatisticsModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int StatisticsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_summary.rows.size();
}

int StatisticsModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant StatisticsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_summary.rows.size())
        return {};
    const StatisticsRow &row = m_summary.rows.at(index.row());
    if (role == Qt::TextAlignmentRole)
        return Qt::AlignCenter;
    if (role != Qt::DisplayRole)
        return {};
    switch (index.column()) {
    case TrainColumn:
        return row.trainNumber;
    case RouteColumn:
        return row.route;
    case SoldColumn:
        return row.soldCount;
    case RefundedColumn:
        return row.refundedCount;
    case RevenueColumn:
        return QStringLiteral("¥%1.%2").arg(row.netRevenueCents / 100).arg(row.netRevenueCents % 100, 2, 10, QLatin1Char('0'));
    case RemainingRateColumn:
        return QStringLiteral("%1%").arg(row.remainingRate * 100.0, 0, 'f', 1);
    default:
        return {};
    }
}

QVariant StatisticsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QAbstractTableModel::headerData(section, orientation, role);
    static const QStringList headers = {QStringLiteral("车次"), QStringLiteral("区间"), QStringLiteral("售票数"),
                                        QStringLiteral("退票数"), QStringLiteral("净收入"), QStringLiteral("余票率")};
    return section >= 0 && section < headers.size() ? headers.at(section) : QVariant{};
}

void StatisticsModel::setSummary(StatisticsSummary summary)
{
    beginResetModel();
    m_summary = std::move(summary);
    endResetModel();
}

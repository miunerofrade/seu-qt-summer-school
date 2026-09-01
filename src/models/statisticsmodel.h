#ifndef STATISTICSMODEL_H
#define STATISTICSMODEL_H

#include "services/statisticsservice.h"

#include <QAbstractTableModel>

class StatisticsModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column
    {
        TrainColumn,
        RouteColumn,
        SoldColumn,
        RefundedColumn,
        RevenueColumn,
        RemainingRateColumn,
        ColumnCount
    };

    explicit StatisticsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void setSummary(StatisticsSummary summary);

private:
    StatisticsSummary m_summary;
};

#endif // STATISTICSMODEL_H

#ifndef TRAINQUERYMODEL_H
#define TRAINQUERYMODEL_H

#include "services/queryservice.h"

#include <QAbstractTableModel>

class TrainQueryModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column
    {
        TrainNumberColumn,
        DepartureStationColumn,
        ArrivalStationColumn,
        DepartureTimeColumn,
        DurationColumn,
        SeatTypeColumn,
        RemainingSeatsColumn,
        PriceColumn,
        ColumnCount
    };

    explicit TrainQueryModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void setRows(QVector<TrainQueryRow> rows);
    const TrainQueryRow *rowAt(int row) const;

private:
    QVector<TrainQueryRow> m_rows;
};

#endif // TRAINQUERYMODEL_H

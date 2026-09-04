#ifndef TRAINQUERYMODEL_H
#define TRAINQUERYMODEL_H

#include "features/query/queryservice.h"

#include <QAbstractTableModel>
#include <QStringList>

class TrainQueryModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column
    {
        TrainNumberColumn,
        ServiceDateColumn,
        OriginStationColumn,
        TerminalStationColumn,
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

    enum Role
    {
        TrainNumberRole = Qt::UserRole,
        SeatTypeRole,
        RemainingSeatsRole,
        PriceRole,
        DepartureTimeRole,
        SeatOptionsRole
    };

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    void setRows(QVector<TrainQueryRow> rows);
    void setHiddenTrainNumbers(const QStringList &trainNumbers);
    const TrainQueryRow *rowAt(int row) const;
    const TrainSeatOption *selectedSeatAt(int row) const;
    void selectSeatType(const QString &seatType, bool availableOnly);

private:
    QVector<TrainQueryRow> m_rows;
    QVector<int> m_selectedSeats;
    QStringList m_hiddenTrainNumbers;
};

#endif // TRAINQUERYMODEL_H

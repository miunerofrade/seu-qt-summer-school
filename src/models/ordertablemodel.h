#ifndef ORDERTABLEMODEL_H
#define ORDERTABLEMODEL_H

#include "services/orderservice.h"

#include <QAbstractTableModel>

class DataStore;

class OrderTableModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column
    {
        OrderNumberColumn,
        PassengerColumn,
        TrainColumn,
        DepartureDateColumn,
        SeatTypeColumn,
        PriceColumn,
        StatusColumn,
        ColumnCount
    };

    explicit OrderTableModel(DataStore *dataStore, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section,
                        Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    const OrderSummary *rowAt(int row) const;
    QString searchableText(int row) const;

private:
    void reload();

    DataStore *m_dataStore;
    QVector<OrderSummary> m_rows;
};

#endif // ORDERTABLEMODEL_H

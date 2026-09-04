#ifndef PASSENGERTABLEMODEL_H
#define PASSENGERTABLEMODEL_H

#include <QAbstractTableModel>

class DataStore;

namespace domain {
struct Passenger;
}

class PassengerTableModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column
    {
        NameColumn,
        DocumentTypeColumn,
        DocumentNumberColumn,
        OwnerColumn,
        ColumnCount
    };

    explicit PassengerTableModel(DataStore *dataStore, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section,
                        Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    const domain::Passenger *passengerAt(int row) const;
    QString searchableText(int row) const;

private:
    void reload();

    DataStore *m_dataStore;
    QVector<int> m_rows;
};

#endif // PASSENGERTABLEMODEL_H

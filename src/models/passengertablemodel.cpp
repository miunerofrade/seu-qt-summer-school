#include "models/passengertablemodel.h"

#include "data/datastore.h"
#include "services/passengerservice.h"

PassengerTableModel::PassengerTableModel(DataStore *dataStore, QObject *parent)
    : QAbstractTableModel(parent)
    , m_dataStore(dataStore)
{
    connect(m_dataStore, &DataStore::dataChanged, this, [this]() { reload(); });
}

int PassengerTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_dataStore->data().passengers.size();
}

int PassengerTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant PassengerTableModel::data(const QModelIndex &index, int role) const
{
    const domain::Passenger *passenger = passengerAt(index.row());
    if (!index.isValid() || !passenger)
        return {};

    if (role == Qt::UserRole)
        return passenger->id;
    if (role == Qt::TextAlignmentRole)
        return Qt::AlignCenter;
    if (role != Qt::DisplayRole)
        return {};

    switch (index.column()) {
    case NameColumn:
        return passenger->name;
    case DocumentTypeColumn:
        return passenger->documentType;
    case DocumentNumberColumn:
        return PassengerService::maskedDocumentNumber(passenger->documentNumber);
    default:
        return {};
    }
}

QVariant PassengerTableModel::headerData(int section,
                                         Qt::Orientation orientation,
                                         int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QAbstractTableModel::headerData(section, orientation, role);
    switch (section) {
    case NameColumn:
        return tr("姓名");
    case DocumentTypeColumn:
        return tr("证件类型");
    case DocumentNumberColumn:
        return tr("证件号码");
    default:
        return {};
    }
}

const domain::Passenger *PassengerTableModel::passengerAt(int row) const
{
    const auto &passengers = m_dataStore->data().passengers;
    return row >= 0 && row < passengers.size() ? &passengers.at(row) : nullptr;
}

QString PassengerTableModel::searchableText(int row) const
{
    const domain::Passenger *passenger = passengerAt(row);
    if (!passenger)
        return {};
    return passenger->name + QLatin1Char('\n') + passenger->documentType + QLatin1Char('\n')
        + passenger->documentNumber;
}

void PassengerTableModel::reload()
{
    beginResetModel();
    endResetModel();
}

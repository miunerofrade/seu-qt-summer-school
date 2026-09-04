#include "models/passengertablemodel.h"

#include "data/datastore.h"
#include "services/passengerservice.h"

PassengerTableModel::PassengerTableModel(DataStore *dataStore, QObject *parent)
    : QAbstractTableModel(parent)
    , m_dataStore(dataStore)
{
    connect(m_dataStore, &DataStore::dataChanged, this, [this]() { reload(); });
    reload();
}

int PassengerTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
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
    case OwnerColumn:
        return m_dataStore->usernameFor(passenger->ownerUserId);
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
    case OwnerColumn:
        return tr("所属账号");
    default:
        return {};
    }
}

const domain::Passenger *PassengerTableModel::passengerAt(int row) const
{
    const auto &passengers = m_dataStore->data().passengers;
    return row >= 0 && row < m_rows.size() ? &passengers.at(m_rows.at(row)) : nullptr;
}

QString PassengerTableModel::searchableText(int row) const
{
    const domain::Passenger *passenger = passengerAt(row);
    if (!passenger)
        return {};
    return passenger->name + QLatin1Char('\n') + passenger->documentType + QLatin1Char('\n')
        + passenger->documentNumber + QLatin1Char('\n') + m_dataStore->usernameFor(passenger->ownerUserId);
}

void PassengerTableModel::reload()
{
    beginResetModel();
    m_rows.clear();
    const auto &passengers = m_dataStore->data().passengers;
    for (int i = 0; i < passengers.size(); ++i)
        if (m_dataStore->canAccessOwner(passengers.at(i).ownerUserId))
            m_rows.append(i);
    endResetModel();
}

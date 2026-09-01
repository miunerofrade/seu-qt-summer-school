#include "services/passengerservice.h"

#include "data/datastore.h"

#include <QUuid>

#include <algorithm>

PassengerService::PassengerService(DataStore *dataStore)
    : m_dataStore(dataStore)
{
}

OperationResult PassengerService::addPassenger(const QString &name,
                                               const QString &documentType,
                                               const QString &documentNumber,
                                               QString *createdId)
{
    const QString normalizedName = name.trimmed();
    const QString normalizedType = documentType.trimmed();
    const QString normalizedNumber = documentNumber.trimmed();
    const OperationResult validation = validate(normalizedName, normalizedType, normalizedNumber);
    if (!validation)
        return validation;

    domain::AppData candidate = m_dataStore->data();
    domain::Passenger passenger;
    passenger.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    passenger.name = normalizedName;
    passenger.documentType = normalizedType;
    passenger.documentNumber = normalizedNumber;
    candidate.passengers.append(passenger);

    const OperationResult result = m_dataStore->commit(std::move(candidate));
    if (result && createdId)
        *createdId = passenger.id;
    return result;
}

OperationResult PassengerService::updatePassenger(const QString &id,
                                                  const QString &name,
                                                  const QString &documentType,
                                                  const QString &documentNumber)
{
    const QString normalizedName = name.trimmed();
    const QString normalizedType = documentType.trimmed();
    const QString normalizedNumber = documentNumber.trimmed();
    const OperationResult validation = validate(normalizedName, normalizedType, normalizedNumber, id);
    if (!validation)
        return validation;

    domain::AppData candidate = m_dataStore->data();
    auto passenger = std::find_if(candidate.passengers.begin(),
                                  candidate.passengers.end(),
                                  [&id](const domain::Passenger &item) { return item.id == id; });
    if (passenger == candidate.passengers.end())
        return OperationResult::failure(QObject::tr("所选乘车人已不存在。"));

    passenger->name = normalizedName;
    passenger->documentType = normalizedType;
    passenger->documentNumber = normalizedNumber;
    return m_dataStore->commit(std::move(candidate));
}

OperationResult PassengerService::removePassenger(const QString &id)
{
    const auto &data = m_dataStore->data();
    const bool hasActiveTicket = std::any_of(data.tickets.cbegin(),
                                             data.tickets.cend(),
                                             [&id](const domain::Ticket &ticket) {
                                                 return ticket.passengerId == id
                                                     && ticket.status == domain::TicketStatus::Issued;
                                             });
    if (hasActiveTicket)
        return OperationResult::failure(QObject::tr("该乘车人存在未完成车票，不能删除。"));

    domain::AppData candidate = data;
    const auto passenger = std::find_if(candidate.passengers.begin(),
                                        candidate.passengers.end(),
                                        [&id](const domain::Passenger &item) { return item.id == id; });
    if (passenger == candidate.passengers.end())
        return OperationResult::failure(QObject::tr("所选乘车人已不存在。"));

    candidate.passengers.erase(passenger);
    return m_dataStore->commit(std::move(candidate));
}

QString PassengerService::maskedDocumentNumber(const QString &documentNumber)
{
    const int length = documentNumber.size();
    if (length <= 0)
        return {};
    if (length <= 4)
        return QString(length, QLatin1Char('*'));
    if (length <= 8)
        return documentNumber.left(2)
            + QString(length - 4, QLatin1Char('*'))
            + documentNumber.right(2);
    return documentNumber.left(4)
        + QString(length - 8, QLatin1Char('*'))
        + documentNumber.right(4);
}

OperationResult PassengerService::validate(const QString &name,
                                           const QString &documentType,
                                           const QString &documentNumber,
                                           const QString &excludedId) const
{
    if (name.isEmpty())
        return OperationResult::failure(QObject::tr("姓名不能为空。"));
    if (documentType.isEmpty())
        return OperationResult::failure(QObject::tr("证件类型不能为空。"));
    if (documentNumber.isEmpty())
        return OperationResult::failure(QObject::tr("证件号码不能为空。"));

    const auto &passengers = m_dataStore->data().passengers;
    const bool duplicate = std::any_of(passengers.cbegin(),
                                       passengers.cend(),
                                       [&documentNumber, &excludedId](const domain::Passenger &passenger) {
                                           return passenger.id != excludedId
                                               && passenger.documentNumber.compare(
                                                      documentNumber,
                                                      Qt::CaseInsensitive)
                                                      == 0;
                                       });
    if (duplicate)
        return OperationResult::failure(QObject::tr("该证件号码已经存在。"));
    return OperationResult::ok();
}

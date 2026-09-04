#include "features/passengers/passengerservice.h"

#include "data/datastore.h"

#include <QDate>
#include <QUuid>

#include <algorithm>

namespace {
bool validChineseIdCard(const QString &number)
{
    if (number.size() != 18)
        return false;
    for (int i = 0; i < 17; ++i) {
        if (number.at(i) < QLatin1Char('0') || number.at(i) > QLatin1Char('9'))
            return false;
    }
    const QChar check = number.at(17).toUpper();
    if (!((check >= QLatin1Char('0') && check <= QLatin1Char('9')) || check == QLatin1Char('X')))
        return false;
    if (!QDate::fromString(number.mid(6, 8), QStringLiteral("yyyyMMdd")).isValid())
        return false;

    static constexpr int weights[17] = {7, 9, 10, 5, 8, 4, 2, 1, 6,
                                        3, 7, 9, 10, 5, 8, 4, 2};
    static constexpr char checkCodes[] = "10X98765432";
    int sum = 0;
    for (int i = 0; i < 17; ++i)
        sum += number.at(i).digitValue() * weights[i];
    return check == QLatin1Char(checkCodes[sum % 11]);
}
} // 命名空间

bool PassengerService::isValidChineseIdCard(const QString &documentNumber)
{
    return validChineseIdCard(documentNumber);
}

PassengerService::PassengerService(DataStore *dataStore)
    : m_dataStore(dataStore)
{
}

OperationResult PassengerService::addPassenger(const QString &name,
                                               const QString &documentType,
                                               const QString &documentNumber,
                                               QString *createdId)
{
    if (m_dataStore->currentUserId().isEmpty())
        return OperationResult::failure(QObject::tr("请先登录。"));
    const QString normalizedName = name.trimmed();
    const QString normalizedType = documentType.trimmed();
    QString normalizedNumber = documentNumber.trimmed();
    if (normalizedType == QStringLiteral("身份证"))
        normalizedNumber = normalizedNumber.toUpper();
    const OperationResult validation = validate(normalizedName, normalizedType, normalizedNumber);
    if (!validation)
        return validation;

    domain::AppData candidate = m_dataStore->data();
    domain::Passenger passenger;
    passenger.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    passenger.name = normalizedName;
    passenger.documentType = normalizedType;
    passenger.documentNumber = normalizedNumber;
    passenger.ownerUserId = m_dataStore->currentUserId();
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
    QString normalizedNumber = documentNumber.trimmed();
    if (normalizedType == QStringLiteral("身份证"))
        normalizedNumber = normalizedNumber.toUpper();
    const OperationResult validation = validate(normalizedName, normalizedType, normalizedNumber, id);
    if (!validation)
        return validation;

    domain::AppData candidate = m_dataStore->data();
    auto passenger = std::find_if(candidate.passengers.begin(),
                                  candidate.passengers.end(),
                                  [&id](const domain::Passenger &item) { return item.id == id; });
    if (passenger == candidate.passengers.end())
        return OperationResult::failure(QObject::tr("所选乘车人已不存在。"));

    if (!m_dataStore->canAccessOwner(passenger->ownerUserId))
        return OperationResult::failure(QObject::tr("无权操作该乘车人。"));

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

    if (!m_dataStore->canAccessOwner(passenger->ownerUserId))
        return OperationResult::failure(QObject::tr("无权操作该乘车人。"));

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
    if (name.size() > 20)
        return OperationResult::failure(QObject::tr("姓名长度不能超过 20 个字符。"));
    if (documentType.isEmpty())
        return OperationResult::failure(QObject::tr("证件类型不能为空。"));
    if (documentNumber.isEmpty())
        return OperationResult::failure(QObject::tr("证件号码不能为空。"));
    if (documentType == QStringLiteral("身份证") && !isValidChineseIdCard(documentNumber))
        return OperationResult::failure(QObject::tr("身份证号码必须为有效的 18 位号码（含正确校验码）。"));

    const auto &passengers = m_dataStore->data().passengers;
    QString owner = m_dataStore->currentUserId();
    for (const auto &passenger : passengers) {
        if (passenger.id == excludedId) {
            if (!m_dataStore->canAccessOwner(passenger.ownerUserId))
                return OperationResult::failure(QObject::tr("无权操作该乘车人。"));
            owner = passenger.ownerUserId;
        }
    }
    const bool duplicate = std::any_of(passengers.cbegin(),
                                       passengers.cend(),
                                       [&documentNumber, &excludedId, &owner](const domain::Passenger &passenger) {
                                           return passenger.ownerUserId == owner && passenger.id != excludedId
                                               && passenger.documentNumber.compare(
                                                      documentNumber,
                                                      Qt::CaseInsensitive)
                                                      == 0;
                                       });
    if (duplicate)
        return OperationResult::failure(QObject::tr("该证件号码已经存在。"));
    return OperationResult::ok();
}

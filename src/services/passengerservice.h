#ifndef PASSENGERSERVICE_H
#define PASSENGERSERVICE_H

#include "common/result.h"

#include <QString>

class DataStore;

class PassengerService final
{
public:
    explicit PassengerService(DataStore *dataStore);

    OperationResult addPassenger(const QString &name,
                                 const QString &documentType,
                                 const QString &documentNumber,
                                 QString *createdId = nullptr);
    OperationResult updatePassenger(const QString &id,
                                    const QString &name,
                                    const QString &documentType,
                                    const QString &documentNumber);
    OperationResult removePassenger(const QString &id);

    static QString maskedDocumentNumber(const QString &documentNumber);

private:
    OperationResult validate(const QString &name,
                             const QString &documentType,
                             const QString &documentNumber,
                             const QString &excludedId = {}) const;

    DataStore *m_dataStore;
};

#endif // PASSENGERSERVICE_H

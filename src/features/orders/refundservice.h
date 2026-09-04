#ifndef REFUNDSERVICE_H
#define REFUNDSERVICE_H

#include "common/result.h"

#include <QDateTime>

class DataStore;

struct RefundQuote
{
    QString ticketId;
    QDateTime departureAt;
    int ratePercent = 0;
    qint64 feeCents = 0;
    qint64 refundAmountCents = 0;
};

struct RefundReceipt : RefundQuote
{
    QString refundRecordId;
};

class RefundService final
{
public:
    explicit RefundService(DataStore *dataStore);

    OperationResult quote(const QString &ticketId,
                          const QDateTime &now,
                          RefundQuote *result) const;
    OperationResult refund(const QString &ticketId,
                           const QDateTime &now = QDateTime::currentDateTime(),
                           RefundReceipt *receipt = nullptr);

private:
    DataStore *m_dataStore;
};

#endif // REFUNDSERVICE_H

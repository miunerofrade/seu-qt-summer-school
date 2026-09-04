#include "features/orders/orderdelegates.h"
#include "common/money.h"

#include "domain/entities.h"
#include "features/orders/orderservice.h"

QString OrderMoneyDelegate::displayText(const QVariant &value, const QLocale &locale) const
{
    Q_UNUSED(locale)
    const qint64 cents = value.toLongLong();
    return common::formatMoney(cents);
}

QString OrderStatusDelegate::displayText(const QVariant &value, const QLocale &locale) const
{
    Q_UNUSED(locale)
    return OrderService::statusText(static_cast<domain::TicketStatus>(value.toInt()));
}

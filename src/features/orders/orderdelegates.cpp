#include "features/orders/orderdelegates.h"

#include "domain/entities.h"
#include "features/orders/orderservice.h"

QString OrderMoneyDelegate::displayText(const QVariant &value, const QLocale &locale) const
{
    Q_UNUSED(locale)
    const qint64 cents = value.toLongLong();
    return QStringLiteral("¥%1.%2")
        .arg(cents / 100)
        .arg(cents % 100, 2, 10, QLatin1Char('0'));
}

QString OrderStatusDelegate::displayText(const QVariant &value, const QLocale &locale) const
{
    Q_UNUSED(locale)
    return OrderService::statusText(static_cast<domain::TicketStatus>(value.toInt()));
}

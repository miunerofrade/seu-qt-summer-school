#ifndef ORDERDELEGATES_H
#define ORDERDELEGATES_H

#include <QStyledItemDelegate>

class OrderMoneyDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QString displayText(const QVariant &value, const QLocale &locale) const override;
};

class OrderStatusDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QString displayText(const QVariant &value, const QLocale &locale) const override;
};

#endif // ORDERDELEGATES_H

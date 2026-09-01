#ifndef PASSENGERFILTERPROXYMODEL_H
#define PASSENGERFILTERPROXYMODEL_H

#include <QSortFilterProxyModel>

class PassengerFilterProxyModel final : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit PassengerFilterProxyModel(QObject *parent = nullptr);
    void setKeyword(const QString &keyword);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString m_keyword;
};

#endif // PASSENGERFILTERPROXYMODEL_H

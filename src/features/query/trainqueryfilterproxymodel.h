#ifndef TRAINQUERYFILTERPROXYMODEL_H
#define TRAINQUERYFILTERPROXYMODEL_H

#include <QSortFilterProxyModel>

class TrainQueryFilterProxyModel final : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit TrainQueryFilterProxyModel(QObject *parent = nullptr);

    void setTrainNumberFilter(const QString &number);
    void setSeatTypeFilter(const QString &seatType);
    void setAvailableOnly(bool availableOnly);
    void setSortByPrice(bool sortByPrice);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    void refreshFilter();

    QString m_trainNumber;
    QString m_seatType;
    bool m_availableOnly = false;
    bool m_sortByPrice = false;
};

#endif // TRAINQUERYFILTERPROXYMODEL_H

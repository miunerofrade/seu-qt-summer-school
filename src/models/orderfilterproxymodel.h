#ifndef ORDERFILTERPROXYMODEL_H
#define ORDERFILTERPROXYMODEL_H

#include <QDate>
#include <QSortFilterProxyModel>

class OrderFilterProxyModel final : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit OrderFilterProxyModel(QObject *parent = nullptr);

    void setKeyword(const QString &keyword);
    void setStatus(int status);
    void setDateRange(const QDate &from, const QDate &to);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    void invalidateRows();

    QString m_keyword;
    int m_status = -1;
    QDate m_from;
    QDate m_to;
};

#endif // ORDERFILTERPROXYMODEL_H

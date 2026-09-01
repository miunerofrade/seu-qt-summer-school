#ifndef ORDERCONTROLLER_H
#define ORDERCONTROLLER_H

#include <QObject>

class DataStore;
class OrderFilterProxyModel;
class OrderTableModel;
class QComboBox;
class QDateEdit;
class QLineEdit;
class QPushButton;
class QTableView;
class QWidget;

class OrderController final : public QObject
{
public:
    OrderController(DataStore *dataStore,
                    QWidget *dialogParent,
                    QTableView *table,
                    QLineEdit *searchEdit,
                    QComboBox *statusCombo,
                    QDateEdit *dateFrom,
                    QDateEdit *dateTo,
                    QPushButton *searchButton,
                    QPushButton *detailsButton,
                    QPushButton *refundButton,
                    QObject *parent = nullptr);

private:
    void applyFilters(bool refreshStatuses);
    QString selectedOrderId() const;
    void openSelectedOrder(bool refundMode);
    QString showOrderDetails(const QString &orderId, bool refundMode);
    void confirmRefund(const QString &ticketId);
    void updateActionButtons();

    DataStore *m_dataStore;
    QWidget *m_dialogParent;
    QTableView *m_table;
    QLineEdit *m_searchEdit;
    QComboBox *m_statusCombo;
    QDateEdit *m_dateFrom;
    QDateEdit *m_dateTo;
    QPushButton *m_detailsButton;
    QPushButton *m_refundButton;
    OrderTableModel *m_model;
    OrderFilterProxyModel *m_proxy;
};

#endif // ORDERCONTROLLER_H

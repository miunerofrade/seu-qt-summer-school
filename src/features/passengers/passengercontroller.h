#ifndef PASSENGERCONTROLLER_H
#define PASSENGERCONTROLLER_H

#include "services/passengerservice.h"

#include <QObject>

class DataStore;
class PassengerFilterProxyModel;
class PassengerTableModel;
class QLineEdit;
class QPushButton;
class QTableView;
class QWidget;

class PassengerController final : public QObject
{
public:
    PassengerController(DataStore *dataStore,
                        QWidget *dialogParent,
                        QTableView *table,
                        QLineEdit *searchEdit,
                        QPushButton *addButton,
                        QPushButton *editButton,
                        QPushButton *deleteButton,
                        QPushButton *searchButton,
                        QObject *parent = nullptr);

private:
    void addPassenger();
    void editSelectedPassenger();
    void deleteSelectedPassenger();
    void filterPassengers();
    QString selectedPassengerId() const;

    PassengerService m_service;
    QWidget *m_dialogParent;
    QTableView *m_table;
    QLineEdit *m_searchEdit;
    PassengerTableModel *m_model;
    PassengerFilterProxyModel *m_proxy;
};

#endif // PASSENGERCONTROLLER_H

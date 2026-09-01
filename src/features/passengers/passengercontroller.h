#ifndef PASSENGERCONTROLLER_H
#define PASSENGERCONTROLLER_H

#include <QObject>

class QLineEdit;
class QPushButton;
class QTableWidget;
class QWidget;

class PassengerController final : public QObject
{
public:
    PassengerController(QWidget *dialogParent,
                        QTableWidget *table,
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

    QWidget *m_dialogParent;
    QTableWidget *m_table;
    QLineEdit *m_searchEdit;
};

#endif // PASSENGERCONTROLLER_H

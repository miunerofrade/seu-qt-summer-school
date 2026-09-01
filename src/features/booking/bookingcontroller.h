#ifndef BOOKINGCONTROLLER_H
#define BOOKINGCONTROLLER_H

#include <QObject>

class DataStore;
class QComboBox;
class QDateEdit;
class QPushButton;
class QTableView;

class BookingController final : public QObject
{
public:
    BookingController(DataStore *dataStore,
                      QTableView *table,
                      QComboBox *departureStation,
                      QComboBox *arrivalStation,
                      QDateEdit *travelDate,
                      QPushButton *bookButton,
                      QObject *parent = nullptr);

private:
    void bookSelected();

    DataStore *m_dataStore;
    QTableView *m_table;
    QComboBox *m_departureStation;
    QComboBox *m_arrivalStation;
    QDateEdit *m_travelDate;
};

#endif // BOOKINGCONTROLLER_H

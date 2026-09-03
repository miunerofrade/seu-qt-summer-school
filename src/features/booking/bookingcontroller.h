#ifndef BOOKINGCONTROLLER_H
#define BOOKINGCONTROLLER_H

#include <QObject>

class DataStore;
class QPushButton;
class QTableView;

class BookingController final : public QObject
{
public:
    BookingController(DataStore *dataStore,
                      QTableView *table,
                      QPushButton *bookButton,
                      QObject *parent = nullptr);

private:
    void bookSelected();

    DataStore *m_dataStore;
    QTableView *m_table;
};

#endif // BOOKINGCONTROLLER_H

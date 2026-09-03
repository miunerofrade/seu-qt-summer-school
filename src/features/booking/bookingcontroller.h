#ifndef BOOKINGCONTROLLER_H
#define BOOKINGCONTROLLER_H

#include <QObject>

class DataStore;
class QPushButton;
class QTableView;
class RailwayQueryService;

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
    QPushButton *m_bookButton;
    RailwayQueryService *m_railwayService;
};

#endif // BOOKINGCONTROLLER_H

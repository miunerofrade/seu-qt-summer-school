#ifndef ADMINCONTROLLER_H
#define ADMINCONTROLLER_H

#include <QObject>

class DataStore;
class QLabel;
class QLineEdit;
class QPushButton;
class QWidget;

struct AdminWidgets
{
    QWidget *dialogParent = nullptr;
    QLineEdit *searchEdit = nullptr;
    QPushButton *recentButton = nullptr;
    QPushButton *addDataButton = nullptr;
    QPushButton *manageStationsButton = nullptr;
    QPushButton *manageTrainsButton = nullptr;
    QPushButton *manageSchedulesButton = nullptr;
    QPushButton *manageSeatsButton = nullptr;
    QLabel *stationCountLabel = nullptr;
    QLabel *trainCountLabel = nullptr;
    QLabel *scheduleCountLabel = nullptr;
    QLabel *seatCountLabel = nullptr;
    QWidget *stationCard = nullptr;
    QWidget *trainCard = nullptr;
    QWidget *scheduleCard = nullptr;
    QWidget *seatCard = nullptr;
    QWidget *recentCard = nullptr;
};

class AdminController final : public QObject
{
public:
    AdminController(DataStore *dataStore, AdminWidgets widgets, QObject *parent = nullptr);

private:
    void refreshCounts();
    void filterCards();
    void showAddMenu();

    DataStore *m_dataStore;
    AdminWidgets m_widgets;
};

#endif // ADMINCONTROLLER_H

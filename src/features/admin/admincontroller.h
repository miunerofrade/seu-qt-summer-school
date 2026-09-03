#ifndef ADMINCONTROLLER_H
#define ADMINCONTROLLER_H

#include <QObject>

class DataStore;
class QLabel;
class QPushButton;
class QWidget;

struct AdminWidgets
{
    QWidget *dialogParent = nullptr;
    QPushButton *manageStationsButton = nullptr;
    QPushButton *manageTrainsButton = nullptr;
    QPushButton *manageSchedulesButton = nullptr;
    QPushButton *manageSeatsButton = nullptr;
    QPushButton *resetDataButton = nullptr;
    QLabel *backupStatusLabel = nullptr;
};

class AdminController final : public QObject
{
public:
    AdminController(DataStore *dataStore, AdminWidgets widgets, QObject *parent = nullptr);

private:
    void refreshCounts();
    void resetData();

    DataStore *m_dataStore;
    AdminWidgets m_widgets;
};

#endif // ADMINCONTROLLER_H

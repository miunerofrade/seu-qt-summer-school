#ifndef MANAGEMENTDIALOGS_H
#define MANAGEMENTDIALOGS_H

class DataStore;
class QWidget;

void showStationManagementDialog(QWidget *parent, DataStore *dataStore);
void showTrainManagementDialog(QWidget *parent, DataStore *dataStore);
void showScheduleManagementDialog(QWidget *parent, DataStore *dataStore);
void showSeatManagementDialog(QWidget *parent, DataStore *dataStore);

#endif // MANAGEMENTDIALOGS_H

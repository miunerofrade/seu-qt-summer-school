#ifndef QUERYCONTROLLER_H
#define QUERYCONTROLLER_H

#include <QObject>

#include "services/queryservice.h"

class DataStore;
class QCheckBox;
class QComboBox;
class QDateEdit;
class QPushButton;
class QTableView;

class QueryController final : public QObject
{
public:
    QueryController(DataStore *dataStore,
                    QTableView *table,
                    QComboBox *departureStation,
                    QComboBox *arrivalStation,
                    QDateEdit *travelDate,
                    QPushButton *searchButton,
                    QComboBox *trainFilter,
                    QComboBox *seatFilter,
                    QCheckBox *availableOnly,
                    QComboBox *sortCombo,
                    QObject *parent = nullptr);

private:
    void loadStations();
    void loadFilterOptions();
    void executeQuery();
    void applyFilters();

    DataStore *m_dataStore;
    QTableView *m_table;
    QComboBox *m_departureStation;
    QComboBox *m_arrivalStation;
    QDateEdit *m_travelDate;
    QComboBox *m_trainFilter;
    QComboBox *m_seatFilter;
    QCheckBox *m_availableOnly;
    QComboBox *m_sortCombo;
    class TrainQueryModel *m_model;
    class TrainQueryFilterProxyModel *m_proxy;
    QueryService m_service;
};

#endif // QUERYCONTROLLER_H

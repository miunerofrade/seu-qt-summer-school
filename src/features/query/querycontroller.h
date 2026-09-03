#ifndef QUERYCONTROLLER_H
#define QUERYCONTROLLER_H

#include <QObject>

#include "services/railwayqueryservice.h"

class DataStore;
class QCheckBox;
class QComboBox;
class QDateEdit;
class QEvent;
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

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void loadStations(const QVector<RailwayStation> &stations);
    bool isOfficialStation(const QString &code) const;
    void loadFilterOptions(const QVector<TrainQueryRow> &rows);
    void executeQuery();
    void showCachedTrains();
    void showRows(QVector<TrainQueryRow> rows);
    void applyFilters();
    void resizeColumnsToViewport();

    DataStore *m_dataStore;
    QTableView *m_table;
    bool m_officialStationsReady = false;
    bool m_resizingColumns = false;
    QVector<int> m_minimumColumnWidths;
    QComboBox *m_departureStation;
    QComboBox *m_arrivalStation;
    QDateEdit *m_travelDate;
    QPushButton *m_searchButton;
    QComboBox *m_trainFilter;
    QComboBox *m_seatFilter;
    QCheckBox *m_availableOnly;
    QComboBox *m_sortCombo;
    class TrainQueryModel *m_model;
    class TrainQueryFilterProxyModel *m_proxy;
    RailwayQueryService *m_railwayService;
};

#endif // QUERYCONTROLLER_H

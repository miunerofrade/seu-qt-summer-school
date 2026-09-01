#ifndef STATISTICSCONTROLLER_H
#define STATISTICSCONTROLLER_H

#include "services/statisticsservice.h"

#include <QObject>

class DataStore;
class QComboBox;
class QDateEdit;
class QLabel;
class QPushButton;
class QTableView;
class QWidget;
class StatisticsModel;

class StatisticsController final : public QObject
{
public:
    StatisticsController(DataStore *dataStore, QWidget *filterCard, QWidget *quickFilterGroup,
                         QWidget *dateFilterGroup, QWidget *trainFilterGroup, QWidget *stationFilterGroup,
                         QWidget *seatFilterGroup, QWidget *actionGroup, QComboBox *periodCombo,
                         QDateEdit *dateFrom, QDateEdit *dateTo, QComboBox *trainCombo,
                         QComboBox *stationCombo, QComboBox *seatCombo, QPushButton *queryButton,
                         QPushButton *exportButton, QLabel *soldValue, QLabel *soldCaption,
                         QLabel *refundedValue, QLabel *refundedCaption, QLabel *revenueValue,
                         QLabel *revenueCaption, QLabel *rateValue, QLabel *rateCaption,
                         QTableView *trainTable, QObject *parent = nullptr);

private:
    void populateOptions();
    void applyQuickPeriod();
    void refresh();
    void updateMetrics(const StatisticsSummary &summary);

    DataStore *m_dataStore;
    QComboBox *m_periodCombo;
    QDateEdit *m_dateFrom;
    QDateEdit *m_dateTo;
    QComboBox *m_trainCombo;
    QComboBox *m_stationCombo;
    QComboBox *m_seatCombo;
    QLabel *m_soldValue;
    QLabel *m_soldCaption;
    QLabel *m_refundedValue;
    QLabel *m_refundedCaption;
    QLabel *m_revenueValue;
    QLabel *m_revenueCaption;
    QLabel *m_rateValue;
    QLabel *m_rateCaption;
    StatisticsModel *m_model;
};

#endif // STATISTICSCONTROLLER_H

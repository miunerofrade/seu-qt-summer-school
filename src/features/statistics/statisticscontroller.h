#ifndef STATISTICSCONTROLLER_H
#define STATISTICSCONTROLLER_H

#include <QObject>

class QTableView;
class QWidget;

class StatisticsController final : public QObject
{
public:
    StatisticsController(QWidget *filterCard,
                         QWidget *quickFilterGroup,
                         QWidget *dateFilterGroup,
                         QWidget *trainFilterGroup,
                         QWidget *stationFilterGroup,
                         QWidget *actionGroup,
                         QTableView *trainTable,
                         QObject *parent = nullptr);
};

#endif // STATISTICSCONTROLLER_H

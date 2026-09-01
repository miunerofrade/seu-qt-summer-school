#ifndef QUERYCONTROLLER_H
#define QUERYCONTROLLER_H

#include <QObject>

class QTableView;

class QueryController final : public QObject
{
public:
    explicit QueryController(QTableView *table, QObject *parent = nullptr);
};

#endif // QUERYCONTROLLER_H

#ifndef ADMINCONTROLLER_H
#define ADMINCONTROLLER_H

#include <QObject>

class QLineEdit;
class QTableView;

class AdminController final : public QObject
{
public:
    AdminController(QLineEdit *searchEdit, QTableView *recentTable, QObject *parent = nullptr);
};

#endif // ADMINCONTROLLER_H

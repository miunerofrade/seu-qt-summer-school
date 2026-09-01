#ifndef DATEEDIT_H
#define DATEEDIT_H

#include <QDateEdit>
#include <QList>

class DateEdit : public QDateEdit
{
public:
    explicit DateEdit(QWidget *parent = nullptr);

private:
    void updateAdjacentMonthFormats(int year, int month);

    QList<QDate> m_adjacentMonthDates;
};

#endif // DATEEDIT_H

#include "features/query/querycontroller.h"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QStandardItemModel>
#include <QTableView>

QueryController::QueryController(QTableView *table, QObject *parent)
    : QObject(parent)
{
    auto *model = new QStandardItemModel(0, 8, this);
    model->setHorizontalHeaderLabels({
        tr("车次"), tr("出发站"), tr("到达站"), tr("出发时间"),
        tr("历时"), tr("席别"), tr("余票"), tr("票价")
    });
    table->setModel(model);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);
}

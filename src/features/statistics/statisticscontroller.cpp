#include "features/statistics/statisticscontroller.h"

#include "widgets/flowlayout.h"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QLayout>
#include <QStandardItemModel>
#include <QTableView>
#include <QWidget>

StatisticsController::StatisticsController(QWidget *filterCard,
                                           QWidget *quickFilterGroup,
                                           QWidget *dateFilterGroup,
                                           QWidget *trainFilterGroup,
                                           QWidget *stationFilterGroup,
                                           QWidget *actionGroup,
                                           QTableView *trainTable,
                                           QObject *parent)
    : QObject(parent)
{
    QLayout *placeholderLayout = filterCard->layout();
    while (QLayoutItem *item = placeholderLayout->takeAt(0))
        delete item;
    delete placeholderLayout;

    auto *filterFlow = new FlowLayout(filterCard, 0, 12, 10);
    filterFlow->setContentsMargins(16, 12, 16, 12);
    filterFlow->addWidget(quickFilterGroup);
    filterFlow->addWidget(dateFilterGroup);
    filterFlow->addWidget(trainFilterGroup);
    filterFlow->addWidget(stationFilterGroup);
    filterFlow->addWidget(actionGroup);

    auto *model = new QStandardItemModel(0, 6, this);
    model->setHorizontalHeaderLabels({
        tr("车次"), tr("区间"), tr("售票数"), tr("退票数"), tr("销售额"), tr("余票率")
    });
    trainTable->setModel(model);
    trainTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    trainTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    trainTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    trainTable->verticalHeader()->setVisible(false);
}

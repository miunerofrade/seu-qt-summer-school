#include "features/admin/admincontroller.h"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QIcon>
#include <QLineEdit>
#include <QStandardItemModel>
#include <QTableView>

AdminController::AdminController(QLineEdit *searchEdit, QTableView *recentTable, QObject *parent)
    : QObject(parent)
{
    searchEdit->addAction(QIcon(":/icons/nav-search.svg"), QLineEdit::LeadingPosition);

    auto *model = new QStandardItemModel(0, 5, this);
    model->setHorizontalHeaderLabels({
        tr("模块"), tr("内容"), tr("操作人"), tr("时间"), tr("状态")
    });
    recentTable->setModel(model);
    recentTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    recentTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    recentTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    recentTable->verticalHeader()->setVisible(false);
}

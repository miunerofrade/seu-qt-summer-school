#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QAbstractItemView>
#include <QHeaderView>
#include <QLineEdit>
#include <QStandardItemModel>
#include <QToolButton>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    auto *ticketModel = new QStandardItemModel(0, 7, this);
    ticketModel->setHorizontalHeaderLabels({
        tr("车次"), tr("出发站"), tr("到达站"), tr("出发时间"),
        tr("历时"), tr("席别"), tr("余票")
    });
    ui->tableTickets->setModel(ticketModel);
    ui->tableTickets->setEditTriggers(QAbstractItemView::NoEditTriggers);

    ui->tableTickets->horizontalHeader()
        ->setSectionResizeMode(QHeaderView::Stretch);

    ui->tableTickets->verticalHeader()->setVisible(false);

    ui->dateTravel->setFocusPolicy(Qt::NoFocus);


    connect(ui->btnQuery, &QToolButton::clicked,this, [this]() {
        ui->stackedWidget->setCurrentWidget(ui->pageQuery);
    });

    connect(ui->btnPassenger, &QToolButton::clicked,this, [this]() {
        ui->stackedWidget->setCurrentWidget(ui->pagePassenger);
    });

    connect(ui->btnOrder, &QToolButton::clicked,this, [this]() {
        ui->stackedWidget->setCurrentWidget(ui->pageOrder);
    });

    connect(ui->btnAdmin, &QToolButton::clicked,this, [this]() {
        ui->stackedWidget->setCurrentWidget(ui->pageAdmin);
    });

    connect(ui->btnStatistics, &QToolButton::clicked,this, [this]() {
        ui->stackedWidget->setCurrentWidget(ui->pageStatistics);
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}

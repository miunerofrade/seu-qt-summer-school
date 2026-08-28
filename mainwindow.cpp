#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QToolButton>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    connect(ui->btnQuery, &QToolButton::clicked,
        this, [this]() {
    ui->stackedWidget->setCurrentWidget(ui->pageQuery);
    });

    connect(ui->btnPassenger, &QToolButton::clicked,
        this, [this]() {
    ui->stackedWidget->setCurrentWidget(ui->pagePassenger);
    });

    connect(ui->btnOrder, &QToolButton::clicked,
        this, [this]() {
    ui->stackedWidget->setCurrentWidget(ui->pageOrder);
    });

    connect(ui->btnAdmin, &QToolButton::clicked,
        this, [this]() {
    ui->stackedWidget->setCurrentWidget(ui->pageAdmin);
    });

    connect(ui->btnStatistics, &QToolButton::clicked,
        this, [this]() {
    ui->stackedWidget->setCurrentWidget(ui->pageStatistics);
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}

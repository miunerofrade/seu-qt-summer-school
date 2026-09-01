#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "flowlayout.h"
#include <QAbstractItemView>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QIcon>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QStandardItemModel>
#include <QTableWidgetItem>
#include <QToolButton>

namespace {
QStringList editPassenger(QWidget *parent, const QStringList &values = {})
{
    QDialog dialog(parent);
    dialog.setWindowTitle(values.isEmpty() ? QObject::tr("新增乘车人") : QObject::tr("编辑乘车人"));
    dialog.resize(460, 340);

    auto *layout = new QFormLayout(&dialog);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setHorizontalSpacing(18);
    layout->setVerticalSpacing(14);
    auto *nameEdit = new QLineEdit(values.value(0), &dialog);
    auto *idTypeEdit = new QComboBox(&dialog);
    idTypeEdit->addItems({QObject::tr("身份证"), QObject::tr("护照"), QObject::tr("港澳通行证")});
    idTypeEdit->setCurrentText(values.value(1, QObject::tr("身份证")));
    auto *idNumberEdit = new QLineEdit(values.value(2), &dialog);
    auto *typeEdit = new QComboBox(&dialog);
    typeEdit->addItems({QObject::tr("成人"), QObject::tr("儿童"), QObject::tr("学生")});
    typeEdit->setCurrentText(values.value(4, QObject::tr("成人")));

    nameEdit->setMinimumWidth(260);
    idTypeEdit->setMinimumWidth(260);
    idNumberEdit->setMinimumWidth(260);
    typeEdit->setMinimumWidth(260);

    layout->addRow(QObject::tr("姓名"), nameEdit);
    layout->addRow(QObject::tr("证件类型"), idTypeEdit);
    layout->addRow(QObject::tr("证件号码"), idNumberEdit);
    layout->addRow(QObject::tr("旅客类型"), typeEdit);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->setCenterButtons(true);
    buttons->button(QDialogButtonBox::Ok)->setText(QObject::tr("确定"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QObject::tr("取消"));
    layout->addRow(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return {};
    }
    return {nameEdit->text(), idTypeEdit->currentText(), idNumberEdit->text(), typeEdit->currentText()};
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 统一主界面操作按钮：常规按钮使用 88 × 28，长文案只按内容扩宽。
    constexpr int standardButtonWidth = 88;
    constexpr int standardButtonHeight = 28;
    constexpr int buttonHorizontalContentPadding = 30;
    const auto buttons = ui->centralwidget->findChildren<QPushButton *>();
    for (QPushButton *button : buttons) {
        button->setFixedHeight(standardButtonHeight);
        button->setMinimumWidth(standardButtonWidth);

        const int contentWidth = button->fontMetrics().horizontalAdvance(button->text())
                                 + buttonHorizontalContentPadding;
        if (contentWidth <= standardButtonWidth)
            button->setFixedWidth(standardButtonWidth);
    }

    // Qt Widgets 没有内置 flex-wrap；用 FlowLayout 按逻辑组自动换行。
    QLayout *placeholderLayout = ui->statisticsFilterCard->layout();
    while (QLayoutItem *item = placeholderLayout->takeAt(0))
        delete item; // 只删除布局项，控件仍由 statisticsFilterCard 持有。
    delete placeholderLayout;

    auto *statisticsFilterFlow = new FlowLayout(ui->statisticsFilterCard, 0, 12, 10);
    statisticsFilterFlow->setContentsMargins(16, 12, 16, 12);
    statisticsFilterFlow->addWidget(ui->statisticsQuickFilterGroup);
    statisticsFilterFlow->addWidget(ui->statisticsDateFilterGroup);
    statisticsFilterFlow->addWidget(ui->statisticsTrainFilterGroup);
    statisticsFilterFlow->addWidget(ui->statisticsStationFilterGroup);
    statisticsFilterFlow->addWidget(ui->statisticsActionGroup);

    auto *ticketModel = new QStandardItemModel(0, 8, this);
    ticketModel->setHorizontalHeaderLabels({
        tr("车次"), tr("出发站"), tr("到达站"), tr("出发时间"),
        tr("历时"), tr("席别"), tr("余票"), tr("票价")
    });
    ui->tableTickets->setModel(ticketModel);
    ui->tableTickets->setEditTriggers(QAbstractItemView::NoEditTriggers);

    ui->tableTickets->horizontalHeader()
        ->setSectionResizeMode(QHeaderView::Stretch);

    ui->tableTickets->verticalHeader()->setVisible(false);

    // 最近维护记录暂时只建立表头，不填充业务数据；后续接入数据源时直接替换模型内容。
    auto *adminRecentModel = new QStandardItemModel(0, 5, this);
    adminRecentModel->setHorizontalHeaderLabels({
        tr("模块"), tr("内容"), tr("操作人"), tr("时间"), tr("状态")
    });
    ui->tableAdminRecent->setModel(adminRecentModel);
    ui->tableAdminRecent->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableAdminRecent->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableAdminRecent->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableAdminRecent->verticalHeader()->setVisible(false);

    // 统计页先建立空模型和表头，业务数据接入后只需向模型填充行。
    auto *statisticsTrainModel = new QStandardItemModel(0, 6, this);
    statisticsTrainModel->setHorizontalHeaderLabels({
        tr("车次"), tr("区间"), tr("售票数"), tr("退票数"), tr("销售额"), tr("余票率")
    });
    ui->tableStatisticsTrains->setModel(statisticsTrainModel);
    ui->tableStatisticsTrains->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableStatisticsTrains->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableStatisticsTrains->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableStatisticsTrains->verticalHeader()->setVisible(false);

    const auto setNavigationIcon = [](QToolButton *button,
                                      const QString &normalPath,
                                      const QString &checkedPath) {
        constexpr int navigationIconSize = 20;
        QIcon icon(normalPath);
        icon.addFile(checkedPath, QSize(navigationIconSize, navigationIconSize), QIcon::Normal, QIcon::On);
        button->setIcon(icon);
        button->setIconSize(QSize(navigationIconSize, navigationIconSize));
    };

    setNavigationIcon(ui->btnQuery, ":/icons/nav-search.svg", ":/icons/nav-search-white.svg");
    setNavigationIcon(ui->btnPassenger, ":/icons/nav-users.svg", ":/icons/nav-users-white.svg");
    setNavigationIcon(ui->btnOrder, ":/icons/nav-orders.svg", ":/icons/nav-orders-white.svg");
    setNavigationIcon(ui->btnAdmin, ":/icons/nav-admin.svg", ":/icons/nav-admin-white.svg");
    setNavigationIcon(ui->btnStatistics, ":/icons/nav-statistics.svg", ":/icons/nav-statistics-white.svg");
    setNavigationIcon(ui->btnSettings, ":/icons/nav-settings.svg", ":/icons/nav-settings-white.svg");

    ui->editPassengerSearch->addAction(QIcon(":/icons/nav-search.svg"), QLineEdit::LeadingPosition);
    ui->editAdminSearch->addAction(QIcon(":/icons/nav-search.svg"), QLineEdit::LeadingPosition);

    ui->tablePassengers->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tablePassengers->verticalHeader()->setVisible(false);
    ui->tablePassengers->setColumnCount(4);

    const auto addPassengerRow = [this](const QStringList &values) {
        const int row = ui->tablePassengers->rowCount();
        ui->tablePassengers->insertRow(row);
        for (int column = 0; column < values.size(); ++column) {
            ui->tablePassengers->setItem(row, column, new QTableWidgetItem(values.at(column)));
        }
    };//待修改，MVD

    connect(ui->btnAddPassenger, &QPushButton::clicked, this, [this, addPassengerRow]() {
        const QStringList values = editPassenger(this);
        if (!values.isEmpty()) {
            addPassengerRow(values);
        }
    });

    connect(ui->btnEditPassenger, &QPushButton::clicked, this, [this]() {
        const int row = ui->tablePassengers->currentRow();
        if (row < 0) {
            return;
        }
        QStringList values;
        for (int column = 0; column < ui->tablePassengers->columnCount(); ++column) {
            values.append(ui->tablePassengers->item(row, column)->text());
        }
        const QStringList editedValues = editPassenger(this, values);
        if (!editedValues.isEmpty()) {
            for (int column = 0; column < editedValues.size(); ++column) {
                ui->tablePassengers->item(row, column)->setText(editedValues.at(column));
            }
        }
    });

    connect(ui->btnDeletePassenger, &QPushButton::clicked, this, [this]() {
        const int row = ui->tablePassengers->currentRow();
        if (row >= 0) {
            ui->tablePassengers->removeRow(row);
        }
    });

    const auto filterPassengers = [this]() {
        const QString keyword = ui->editPassengerSearch->text().trimmed();
        for (int row = 0; row < ui->tablePassengers->rowCount(); ++row) {
            const bool matches = keyword.isEmpty()
                || ui->tablePassengers->item(row, 0)->text().contains(keyword, Qt::CaseInsensitive)
                || ui->tablePassengers->item(row, 2)->text().contains(keyword, Qt::CaseInsensitive);
            ui->tablePassengers->setRowHidden(row, !matches);
        }
    };

    connect(ui->btnSearchPassenger, &QPushButton::clicked, this, filterPassengers);
    connect(ui->editPassengerSearch, &QLineEdit::returnPressed, this, filterPassengers);


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

    connect(ui->btnSettings, &QToolButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentWidget(ui->pageSettings);
    });
}



MainWindow::~MainWindow()
{
    delete ui;
}

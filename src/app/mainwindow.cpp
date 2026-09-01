#include "app/mainwindow.h"
#include "ui_mainwindow.h"

#include "features/admin/admincontroller.h"
#include "features/passengers/passengercontroller.h"
#include "features/query/querycontroller.h"
#include "features/statistics/statisticscontroller.h"

#include <QIcon>
#include <QPushButton>
#include <QToolButton>

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

    new QueryController(ui->tableTickets, this);
    new PassengerController(this,
                            ui->tablePassengers,
                            ui->editPassengerSearch,
                            ui->btnAddPassenger,
                            ui->btnEditPassenger,
                            ui->btnDeletePassenger,
                            ui->btnSearchPassenger,
                            this);
    new AdminController(ui->editAdminSearch, ui->tableAdminRecent, this);
    new StatisticsController(ui->statisticsFilterCard,
                             ui->statisticsQuickFilterGroup,
                             ui->statisticsDateFilterGroup,
                             ui->statisticsTrainFilterGroup,
                             ui->statisticsStationFilterGroup,
                             ui->statisticsActionGroup,
                             ui->tableStatisticsTrains,
                             this);

    const auto setNavigationIcon = [](QToolButton *button,
                                      const QString &normalPath,
                                      const QString &checkedPath) {
        constexpr int navigationIconSize = 20;
        QIcon icon(normalPath);
        icon.addFile(checkedPath,
                     QSize(navigationIconSize, navigationIconSize),
                     QIcon::Normal,
                     QIcon::On);
        button->setIcon(icon);
        button->setIconSize(QSize(navigationIconSize, navigationIconSize));
    };

    setNavigationIcon(ui->btnQuery, ":/icons/nav-search.svg", ":/icons/nav-search-white.svg");
    setNavigationIcon(ui->btnPassenger, ":/icons/nav-users.svg", ":/icons/nav-users-white.svg");
    setNavigationIcon(ui->btnOrder, ":/icons/nav-orders.svg", ":/icons/nav-orders-white.svg");
    setNavigationIcon(ui->btnAdmin, ":/icons/nav-admin.svg", ":/icons/nav-admin-white.svg");
    setNavigationIcon(ui->btnStatistics, ":/icons/nav-statistics.svg", ":/icons/nav-statistics-white.svg");
    setNavigationIcon(ui->btnSettings, ":/icons/nav-settings.svg", ":/icons/nav-settings-white.svg");

    connect(ui->btnQuery, &QToolButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentWidget(ui->pageQuery);
    });
    connect(ui->btnPassenger, &QToolButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentWidget(ui->pagePassenger);
    });
    connect(ui->btnOrder, &QToolButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentWidget(ui->pageOrder);
    });
    connect(ui->btnAdmin, &QToolButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentWidget(ui->pageAdmin);
    });
    connect(ui->btnStatistics, &QToolButton::clicked, this, [this]() {
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

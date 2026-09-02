#include "app/mainwindow.h"
#include "ui_mainwindow.h"

#include "data/datastore.h"
#include "data/jsonrepository.h"
#include "features/admin/admincontroller.h"
#include "features/booking/bookingcontroller.h"
#include "features/orders/ordercontroller.h"
#include "features/passengers/passengercontroller.h"
#include "features/query/querycontroller.h"
#include "features/settings/settingscontroller.h"
#include "features/statistics/statisticscontroller.h"

#include <QDir>
#include <QAction>
#include <QIcon>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QToolButton>

#include <memory>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_dataStore(nullptr)
{
    ui->setupUi(this);

    const QString dataFilePath = QDir(QStringLiteral(QT_SYNC_DATA_DIR))
                                     .filePath(QStringLiteral("app-data.json"));
    m_dataStore = new DataStore(std::make_unique<JsonRepository>(dataFilePath), this);
    const OperationResult initialization = m_dataStore->initialize();

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

    new QueryController(m_dataStore,
                        ui->tableTickets,
                        ui->comboDepartureStation,
                        ui->comboArrivalStation,
                        ui->dateTravel,
                        ui->btnSearch,
                        ui->comboTrainType,
                        ui->comboSeatType,
                        ui->checkAvailableOnly,
                        ui->comboSort,
                        this);
    new BookingController(m_dataStore,
                          ui->tableTickets,
                          ui->comboDepartureStation,
                          ui->comboArrivalStation,
                          ui->dateTravel,
                          ui->btnBookTicket,
                          this);
    new OrderController(m_dataStore,
                        this,
                        ui->tableOrders,
                        ui->editOrderSearch,
                        ui->comboOrderStatus,
                        ui->dateOrderFrom,
                        ui->dateOrderTo,
                        ui->btnSearchOrders,
                        ui->btnOrderDetails,
                        ui->btnRefundOrder,
                        this);
    new PassengerController(m_dataStore,
                            this,
                            ui->tablePassengers,
                            ui->editPassengerSearch,
                            ui->btnAddPassenger,
                            ui->btnEditPassenger,
                            ui->btnDeletePassenger,
                            ui->btnSearchPassenger,
                            this);
    new AdminController(m_dataStore,
                        {this,
                         ui->btnManageStations,
                         ui->btnManageTrains,
                         ui->btnManageSchedules,
                         ui->btnManageSeats,
                         ui->labelStationAdminData,
                         ui->labelTrainAdminData,
                         ui->labelScheduleAdminData,
                         ui->labelSeatAdminData},
                        this);
    new StatisticsController(m_dataStore,
                             ui->statisticsFilterCard,
                             ui->statisticsQuickFilterGroup,
                             ui->statisticsDateFilterGroup,
                             ui->statisticsTrainFilterGroup,
                             ui->statisticsStationFilterGroup,
                             ui->statisticsSeatFilterGroup,
                             ui->statisticsActionGroup,
                             ui->comboStatisticsPeriod,
                             ui->dateStatisticsFrom,
                             ui->dateStatisticsTo,
                             ui->comboStatisticsTrain,
                             ui->comboStatisticsStation,
                             ui->comboStatisticsSeat,
                             ui->btnStatisticsQuery,
                             ui->btnStatisticsExport,
                             ui->labelMetricTicketsValue,
                             ui->labelMetricTicketsCaption,
                             ui->labelMetricRefundsValue,
                             ui->labelMetricRefundsCaption,
                             ui->labelMetricRevenueValue,
                             ui->labelMetricRevenueCaption,
                             ui->labelMetricRateValue,
                             ui->labelMetricRateCaption,
                             ui->statisticsTrendPlaceholder,
                             ui->statisticsSeatPlaceholder,
                             ui->tableStatisticsTrains,
                             this);
    new SettingsController(m_dataStore,
                           this,
                           ui->labelSettingsDataStatus,
                           ui->labelSettingsDataFile,
                           ui->labelSettingsDataDirectory,
                           ui->labelSettingsLastSaved,
                           ui->labelSettingsAutoLoad,
                           ui->btnOpenDataDirectory,
                           ui->btnSaveDataNow,
                           ui->btnReloadData,
                           this);

    setupMenuBar();

    if (!initialization) {
        const QString error = initialization.error;
        QTimer::singleShot(0, this, [this, error]() {
            QMessageBox::critical(
                this,
                tr("数据加载失败"),
                tr("本地数据文件未被覆盖。请检查文件后在设置页重新加载。\n\n%1").arg(error));
        });
    }

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

void MainWindow::setupMenuBar()
{
    ui->menubar->setStyleSheet(QStringLiteral(R"(
QMenuBar {
    background-color: #FFFFFF;
    border-bottom: 1px solid #E5E7EB;
    padding: 2px 6px;
}
QMenuBar::item {
    background: transparent;
    border-radius: 4px;
    padding: 4px 9px;
}
QMenuBar::item:selected {
    background-color: #EDF3FA;
}
)"));

    QMenu *fileMenu = ui->menubar->addMenu(tr("文件(&F)"));
    QAction *saveAction = fileMenu->addAction(tr("保存数据(&S)"));
    saveAction->setShortcut(QKeySequence::Save);
    saveAction->setEnabled(m_dataStore->isWritable());
    connect(saveAction, &QAction::triggered, ui->btnSaveDataNow, &QPushButton::click);
    connect(m_dataStore, &DataStore::statusChanged, saveAction, [this, saveAction]() {
        saveAction->setEnabled(m_dataStore->isWritable());
    });

    QAction *openDirectoryAction = fileMenu->addAction(tr("打开数据目录(&O)"));
    connect(openDirectoryAction, &QAction::triggered, ui->btnOpenDataDirectory, &QPushButton::click);
    QAction *reloadAction = fileMenu->addAction(tr("重新加载数据(&R)"));
    reloadAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+R")));
    connect(reloadAction, &QAction::triggered, ui->btnReloadData, &QPushButton::click);
    fileMenu->addSeparator();
    QAction *exitAction = fileMenu->addAction(tr("退出(&X)"));
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    QMenu *dataMenu = ui->menubar->addMenu(tr("数据(&D)"));
    QAction *addPassengerAction = dataMenu->addAction(tr("新增乘车人(&P)"));
    addPassengerAction->setShortcut(QKeySequence::New);
    connect(addPassengerAction, &QAction::triggered, ui->btnAddPassenger, &QPushButton::click);
    dataMenu->addSeparator();
    QAction *stationAction = dataMenu->addAction(tr("车站管理"));
    connect(stationAction, &QAction::triggered, ui->btnManageStations, &QPushButton::click);
    QAction *trainAction = dataMenu->addAction(tr("车次管理"));
    connect(trainAction, &QAction::triggered, ui->btnManageTrains, &QPushButton::click);
    QAction *scheduleAction = dataMenu->addAction(tr("时刻与经停站管理"));
    connect(scheduleAction, &QAction::triggered, ui->btnManageSchedules, &QPushButton::click);
    QAction *seatAction = dataMenu->addAction(tr("席别、票价与余票管理"));
    connect(seatAction, &QAction::triggered, ui->btnManageSeats, &QPushButton::click);

    QMenu *viewMenu = ui->menubar->addMenu(tr("视图(&V)"));
    const auto addPageAction = [viewMenu](const QString &text, const QKeySequence &shortcut,
                                          QToolButton *button) {
        QAction *action = viewMenu->addAction(text);
        action->setShortcut(shortcut);
        QObject::connect(action, &QAction::triggered, button, &QToolButton::click);
    };
    addPageAction(tr("车票查询"), QKeySequence(QStringLiteral("Ctrl+1")), ui->btnQuery);
    addPageAction(tr("乘车人"), QKeySequence(QStringLiteral("Ctrl+2")), ui->btnPassenger);
    addPageAction(tr("订单"), QKeySequence(QStringLiteral("Ctrl+3")), ui->btnOrder);
    addPageAction(tr("管理"), QKeySequence(QStringLiteral("Ctrl+4")), ui->btnAdmin);
    addPageAction(tr("统计"), QKeySequence(QStringLiteral("Ctrl+5")), ui->btnStatistics);
    addPageAction(tr("设置"), QKeySequence(QStringLiteral("Ctrl+6")), ui->btnSettings);

    QMenu *helpMenu = ui->menubar->addMenu(tr("帮助(&H)"));
    QAction *aboutAction = helpMenu->addAction(tr("关于(&A)"));
    aboutAction->setShortcut(QKeySequence::HelpContents);
    connect(aboutAction, &QAction::triggered, this, [this]() {
        QMessageBox::about(this,
                           tr("关于列车客运售票管理系统"),
                           tr("列车客运售票管理系统\n\n用于车票查询、乘车人及订单管理、运营配置与数据统计。"));
    });
}

#include "app/mainwindow.h"
#include "ui_mainwindow.h"

#include "data/datastore.h"
#include "features/admin/admincontroller.h"
#include "features/booking/bookingcontroller.h"
#include "features/orders/ordercontroller.h"
#include "features/passengers/passengercontroller.h"
#include "features/query/querycontroller.h"
#include "features/settings/settingscontroller.h"
#include "features/statistics/statisticscontroller.h"

#include <QCloseEvent>
#include <QElapsedTimer>
#include <QDebug>
#include <QAction>
#include <QIcon>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QToolButton>

MainWindow::MainWindow(DataStore *dataStore, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_dataStore(dataStore)
{
    QElapsedTimer startupTimer;
    startupTimer.start();
    const auto reportStartup = [&startupTimer](const char *stage) {
        const qint64 elapsed = startupTimer.restart();
        if (qEnvironmentVariableIsSet("QT_SYNC_PROFILE_STARTUP"))
            qInfo("MainWindow %s: %lld ms", stage, elapsed);
    };
    ui->setupUi(this);
    reportStartup("setupUi");

    ui->labelUserModeCaption->setText(m_dataStore->usernameFor(m_dataStore->currentUserId()));
    ui->labelUserModeValue->setText(m_dataStore->isAdmin() ? tr("管理员") : tr("普通用户"));
    ui->labelSettingsAccount->setText(tr("当前账号：%1（%2）")
        .arg(m_dataStore->usernameFor(m_dataStore->currentUserId()), ui->labelUserModeValue->text()));
    ui->labelSettingsSubtitle->setText(m_dataStore->isAdmin()
        ? tr("账号与数据管理") : tr("账号与应用信息"));
    if (!m_dataStore->isAdmin()) {
        ui->btnAdmin->hide();
        ui->btnStatistics->hide();
        ui->settingsDataCard->hide();
        ui->settingsBackupCard->hide();
        ui->stackedWidget->removeWidget(ui->pageAdmin);
        ui->stackedWidget->removeWidget(ui->pageStatistics);
        ui->pageAdmin->hide();
        ui->pageStatistics->hide();
    }
    ui->stackedWidget->setCurrentWidget(ui->pageQuery);

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

    reportStartup("navigation and button layout");
    new QueryController(m_dataStore,
                        ui->tableTickets,
                        ui->comboDepartureStation,
                        ui->comboArrivalStation,
                        ui->dateTravel,
                        ui->btnSearch,
                        ui->comboTrainType,
                        ui->comboSeatType,
                        ui->checkAvailableOnly,
                        ui->checkUseRailwayData,
                        ui->comboSort,
                        this);
    reportStartup("query");
    new BookingController(m_dataStore,
                          ui->tableTickets,
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
    reportStartup("booking, orders, passengers");
    if (m_dataStore->isAdmin()) {
        new AdminController(m_dataStore,
                            {this,
                             ui->btnManageStations,
                             ui->btnManageTrains,
                             ui->btnManageSchedules,
                             ui->btnManageSeats,
                             ui->btnResetAdminData,
                             ui->labelAdminBackupData},
                            this);
        new StatisticsController(m_dataStore,
                                 ui->labelMetricTicketsValue,
                                 ui->labelMetricRefundsValue,
                                 ui->labelMetricRevenueValue,
                                 ui->labelMetricRateValue,
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
                               ui->labelSettingsBackupNote,
                               ui->btnCreateBackup,
                               ui->btnRestoreBackup,
                               this);
    }

    setupMenuBar();
    reportStartup("admin and menus");

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
        if (!m_dataStore->isAdmin()) return;
        ui->stackedWidget->setCurrentWidget(ui->pageAdmin);
    });
    connect(ui->btnStatistics, &QToolButton::clicked, this, [this]() {
        if (!m_dataStore->isAdmin()) return;
        ui->stackedWidget->setCurrentWidget(ui->pageStatistics);
    });
    connect(ui->btnSettings, &QToolButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentWidget(ui->pageSettings);
    });

    // 初始焦点放在窗口上，而不是第一个可编辑的车站字段。
    // 保持输入控件的焦点策略，以支持鼠标点击和键盘导航。
    setFocus(Qt::OtherFocusReason);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QMainWindow::closeEvent(event);
    if (event->isAccepted()) emit closed();
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
    if (m_dataStore->isAdmin()) {
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
    }
    QAction *logoutAction = fileMenu->addAction(tr("退出登录"));
    logoutAction->setObjectName(QStringLiteral("actionLogout"));
    connect(ui->btnLogout, &QPushButton::clicked, logoutAction, &QAction::trigger);
    connect(logoutAction, &QAction::triggered, this, [this]() {
        m_logoutRequested = true;
        close();
    });
    QAction *exitAction = fileMenu->addAction(tr("退出(&X)"));
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    QMenu *dataMenu = ui->menubar->addMenu(tr("数据(&D)"));
    QAction *addPassengerAction = dataMenu->addAction(tr("新增乘车人(&P)"));
    addPassengerAction->setShortcut(QKeySequence::New);
    connect(addPassengerAction, &QAction::triggered, ui->btnAddPassenger, &QPushButton::click);
    if (m_dataStore->isAdmin()) {
        dataMenu->addSeparator();
        QAction *stationAction = dataMenu->addAction(tr("车站管理"));
        connect(stationAction, &QAction::triggered, ui->btnManageStations, &QPushButton::click);
        QAction *trainAction = dataMenu->addAction(tr("车次管理"));
        connect(trainAction, &QAction::triggered, ui->btnManageTrains, &QPushButton::click);
        QAction *scheduleAction = dataMenu->addAction(tr("时刻与经停站管理"));
        connect(scheduleAction, &QAction::triggered, ui->btnManageSchedules, &QPushButton::click);
        QAction *seatAction = dataMenu->addAction(tr("席别、票价与余票管理"));
        connect(seatAction, &QAction::triggered, ui->btnManageSeats, &QPushButton::click);
    }

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
    if (m_dataStore->isAdmin()) {
        addPageAction(tr("管理"), QKeySequence(QStringLiteral("Ctrl+4")), ui->btnAdmin);
        addPageAction(tr("统计"), QKeySequence(QStringLiteral("Ctrl+5")), ui->btnStatistics);
    }
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

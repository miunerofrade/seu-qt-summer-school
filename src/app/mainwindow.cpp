#include "app/mainwindow.h"
#include "ui_mainwindow.h"

#include "data/datastore.h"
#include "data/jsonrepository.h"
#include "features/admin/admincontroller.h"
#include "features/passengers/passengercontroller.h"
#include "features/query/querycontroller.h"
#include "features/settings/settingscontroller.h"
#include "features/statistics/statisticscontroller.h"

#include <QDir>
#include <QIcon>
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
                         ui->editAdminSearch,
                         ui->btnRecentAdmin,
                         ui->btnAddAdminData,
                         ui->btnManageStations,
                         ui->btnManageTrains,
                         ui->btnManageSchedules,
                         ui->btnManageSeats,
                         ui->labelStationAdminData,
                         ui->labelTrainAdminData,
                         ui->labelScheduleAdminData,
                         ui->labelSeatAdminData,
                         ui->cardStationAdmin,
                         ui->cardTrainAdmin,
                         ui->cardScheduleAdmin,
                         ui->cardSeatAdmin,
                         ui->adminRecentCard},
                        this);
    new StatisticsController(ui->statisticsFilterCard,
                             ui->statisticsQuickFilterGroup,
                             ui->statisticsDateFilterGroup,
                             ui->statisticsTrainFilterGroup,
                             ui->statisticsStationFilterGroup,
                             ui->statisticsActionGroup,
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

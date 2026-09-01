#include "features/admin/admincontroller.h"

#include "data/datastore.h"
#include "features/admin/managementdialogs.h"

#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QWidget>

#include <algorithm>

AdminController::AdminController(DataStore *dataStore, AdminWidgets widgets, QObject *parent)
    : QObject(parent)
    , m_dataStore(dataStore)
    , m_widgets(widgets)
{
    m_widgets.searchEdit->addAction(QIcon(":/icons/nav-search.svg"), QLineEdit::LeadingPosition);
    m_widgets.recentButton->setVisible(false);
    m_widgets.recentCard->setVisible(false);

    connect(m_widgets.manageStationsButton, &QPushButton::clicked, this, [this]() {
        showStationManagementDialog(m_widgets.dialogParent, m_dataStore);
    });
    connect(m_widgets.manageTrainsButton, &QPushButton::clicked, this, [this]() {
        showTrainManagementDialog(m_widgets.dialogParent, m_dataStore);
    });
    connect(m_widgets.manageSchedulesButton, &QPushButton::clicked, this, [this]() {
        showScheduleManagementDialog(m_widgets.dialogParent, m_dataStore);
    });
    connect(m_widgets.manageSeatsButton, &QPushButton::clicked, this, [this]() {
        showSeatManagementDialog(m_widgets.dialogParent, m_dataStore);
    });
    connect(m_widgets.addDataButton, &QPushButton::clicked, this, [this]() { showAddMenu(); });
    connect(m_widgets.searchEdit, &QLineEdit::textChanged, this, [this]() { filterCards(); });
    connect(m_dataStore, &DataStore::dataChanged, this, [this]() { refreshCounts(); });
    refreshCounts();
}

void AdminController::refreshCounts()
{
    const domain::AppData &data = m_dataStore->data();
    const int configuredSchedules = std::count_if(data.trains.cbegin(), data.trains.cend(), [](const domain::Train &train) {
        return train.stops.size() >= 2;
    });
    int seatTypes = 0;
    for (const domain::Train &train : data.trains)
        seatTypes += train.seats.size();

    m_widgets.stationCountLabel->setText(tr("%1 个车站").arg(data.stations.size()));
    m_widgets.trainCountLabel->setText(tr("%1 个车次").arg(data.trains.size()));
    m_widgets.scheduleCountLabel->setText(tr("%1 个运行计划已配置").arg(configuredSchedules));
    m_widgets.seatCountLabel->setText(tr("%1 个席别配置").arg(seatTypes));
}

void AdminController::filterCards()
{
    const QString keyword = m_widgets.searchEdit->text().trimmed();
    const auto matches = [&keyword](const QString &text) {
        return keyword.isEmpty() || text.contains(keyword, Qt::CaseInsensitive);
    };
    m_widgets.stationCard->setVisible(matches(tr("车站 站点 城市 站码")));
    m_widgets.trainCard->setVisible(matches(tr("车次 列车 运行日期")));
    m_widgets.scheduleCard->setVisible(matches(tr("时刻 经停站 运行计划")));
    m_widgets.seatCard->setVisible(matches(tr("席别 票价 余票 票额")));
}

void AdminController::showAddMenu()
{
    QMenu menu(m_widgets.addDataButton);
    QAction *stationAction = menu.addAction(tr("新增车站"));
    QAction *trainAction = menu.addAction(tr("新增车次"));
    menu.addSeparator();
    QAction *scheduleAction = menu.addAction(tr("配置时刻与经停站"));
    QAction *seatAction = menu.addAction(tr("配置席别、票价与余票"));
    QAction *selected = menu.exec(m_widgets.addDataButton->mapToGlobal(
        QPoint(0, m_widgets.addDataButton->height())));
    if (selected == stationAction)
        showStationManagementDialog(m_widgets.dialogParent, m_dataStore);
    else if (selected == trainAction)
        showTrainManagementDialog(m_widgets.dialogParent, m_dataStore);
    else if (selected == scheduleAction)
        showScheduleManagementDialog(m_widgets.dialogParent, m_dataStore);
    else if (selected == seatAction)
        showSeatManagementDialog(m_widgets.dialogParent, m_dataStore);
}

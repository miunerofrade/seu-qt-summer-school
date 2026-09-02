#include "features/admin/admincontroller.h"

#include "data/datastore.h"
#include "features/admin/managementdialogs.h"

#include <QLabel>
#include <QPushButton>
#include <QWidget>

#include <algorithm>

AdminController::AdminController(DataStore *dataStore, AdminWidgets widgets, QObject *parent)
    : QObject(parent)
    , m_dataStore(dataStore)
    , m_widgets(widgets)
{
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

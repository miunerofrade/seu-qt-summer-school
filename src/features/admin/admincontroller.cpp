#include "features/admin/admincontroller.h"

#include "data/datastore.h"
#include "features/admin/managementdialogs.h"

#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QWidget>

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
    connect(m_widgets.resetDataButton, &QPushButton::clicked, this, [this]() { resetData(); });
    connect(m_dataStore, &DataStore::dataChanged, this, [this]() { refreshCounts(); });
    connect(m_dataStore, &DataStore::statusChanged, this, [this]() { refreshCounts(); });
    refreshCounts();
}

void AdminController::refreshCounts()
{
    const QFileInfo backup(m_dataStore->backupFilePath());
    m_widgets.backupStatusLabel->setText(
        backup.exists()
            ? tr("基线备份：%1").arg(backup.lastModified().toString(QStringLiteral("MM-dd HH:mm")))
            : tr("基线备份尚未创建"));
    m_widgets.resetDataButton->setEnabled(backup.exists() && m_dataStore->isWritable());
}

void AdminController::resetData()
{
    const auto answer = QMessageBox::warning(
        m_widgets.dialogParent,
        tr("恢复初始数据"),
        tr("将使用基线备份替换当前所有本地业务数据，包括车站、车次、乘车人和演示订单。\n\n"
           "恢复前会自动保存当前数据快照，是否继续？"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    const OperationResult result = m_dataStore->restoreBackup();
    if (!result) {
        QMessageBox::critical(m_widgets.dialogParent, tr("恢复失败"), result.error);
        return;
    }
    QMessageBox::information(
        m_widgets.dialogParent,
        tr("恢复完成"),
        tr("已恢复基线数据。重置前的数据已保存到：\n%1")
            .arg(m_dataStore->lastSafetyBackupPath()));
}

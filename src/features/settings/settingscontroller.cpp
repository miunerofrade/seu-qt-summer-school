#include "features/settings/settingscontroller.h"

#include "data/datastore.h"

#include <QDesktopServices>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QUrl>

SettingsController::SettingsController(DataStore *dataStore,
                                       QWidget *dialogParent,
                                       QLabel *statusLabel,
                                       QLabel *fileLabel,
                                       QLabel *directoryLabel,
                                       QLabel *lastSavedLabel,
                                       QLabel *autoLoadLabel,
                                       QPushButton *openDirectoryButton,
                                       QPushButton *saveButton,
                                       QPushButton *reloadButton,
                                       QLabel *backupNoteLabel,
                                       QPushButton *createBackupButton,
                                       QPushButton *restoreBackupButton,
                                       QObject *parent)
    : QObject(parent)
    , m_dataStore(dataStore)
    , m_dialogParent(dialogParent)
    , m_statusLabel(statusLabel)
    , m_fileLabel(fileLabel)
    , m_directoryLabel(directoryLabel)
    , m_lastSavedLabel(lastSavedLabel)
    , m_autoLoadLabel(autoLoadLabel)
    , m_saveButton(saveButton)
    , m_backupNoteLabel(backupNoteLabel)
    , m_createBackupButton(createBackupButton)
    , m_restoreBackupButton(restoreBackupButton)
{
    connect(openDirectoryButton, &QPushButton::clicked, this, [this]() { openDataDirectory(); });
    connect(saveButton, &QPushButton::clicked, this, [this]() { saveNow(); });
    connect(reloadButton, &QPushButton::clicked, this, [this]() { reloadData(); });
    connect(createBackupButton, &QPushButton::clicked, this, [this]() { createBackup(); });
    connect(restoreBackupButton, &QPushButton::clicked, this, [this]() { restoreBackup(); });
    connect(m_dataStore, &DataStore::statusChanged, this, [this]() { refresh(); });
    refresh();
}

void SettingsController::refresh()
{
    const bool ready = m_dataStore->loadState() == DataStore::LoadState::Ready;
    if (ready && m_dataStore->errorMessage().isEmpty()) {
        m_statusLabel->setText(tr("已加载，可正常读写"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #248A3D; font-weight: 600;"));
    } else if (ready) {
        m_statusLabel->setText(tr("已加载，但上次保存失败：%1").arg(m_dataStore->errorMessage()));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #C47A00; font-weight: 600;"));
    } else if (m_dataStore->loadState() == DataStore::LoadState::Error) {
        m_statusLabel->setText(tr("加载失败：%1").arg(m_dataStore->errorMessage()));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #C9342C; font-weight: 600;"));
    } else {
        m_statusLabel->setText(tr("尚未加载"));
        m_statusLabel->setStyleSheet(QString());
    }

    m_fileLabel->setText(m_dataStore->dataFilePath());
    m_directoryLabel->setText(m_dataStore->dataDirectory());
    m_lastSavedLabel->setText(m_dataStore->lastSavedAt().isValid()
                                  ? m_dataStore->lastSavedAt().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
                                  : tr("—"));
    m_autoLoadLabel->setText(tr("已启用"));
    m_saveButton->setEnabled(m_dataStore->isWritable());
    const QFileInfo backup(m_dataStore->backupFilePath());
    m_backupNoteLabel->setText(
        backup.exists()
            ? tr("基线备份：%1（%2）")
                  .arg(backup.fileName(), backup.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")))
            : tr("尚未创建基线备份"));
    m_createBackupButton->setEnabled(ready);
    m_restoreBackupButton->setEnabled(ready && backup.exists());
}

void SettingsController::openDataDirectory()
{
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(m_dataStore->dataDirectory())))
        QMessageBox::warning(m_dialogParent, tr("打开目录"), tr("无法打开数据目录。"));
}

void SettingsController::saveNow()
{
    const OperationResult result = m_dataStore->save();
    if (!result) {
        QMessageBox::critical(m_dialogParent, tr("保存失败"), result.error);
        return;
    }
    QMessageBox::information(m_dialogParent, tr("保存完成"), tr("业务数据已保存。"));
}

void SettingsController::reloadData()
{
    const auto answer = QMessageBox::question(
        m_dialogParent,
        tr("重新加载数据"),
        tr("重新加载将丢弃尚未保存的内存修改，是否继续？"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    const OperationResult result = m_dataStore->reload();
    if (!result) {
        QMessageBox::critical(m_dialogParent, tr("重新加载失败"), result.error);
        return;
    }
    QMessageBox::information(m_dialogParent, tr("重新加载完成"), tr("已从本地 JSON 恢复业务数据。"));
}

void SettingsController::createBackup()
{
    if (QFileInfo::exists(m_dataStore->backupFilePath())) {
        const auto answer = QMessageBox::question(
            m_dialogParent,
            tr("更新基线备份"),
            tr("这会用当前数据覆盖原基线，之后“恢复初始数据”将恢复到当前状态。是否继续？"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }
    const OperationResult result = m_dataStore->createBackup();
    if (!result) {
        QMessageBox::critical(m_dialogParent, tr("备份失败"), result.error);
        return;
    }
    refresh();
    QMessageBox::information(m_dialogParent, tr("备份完成"), tr("当前数据已保存为新的基线备份。"));
}

void SettingsController::restoreBackup()
{
    const auto answer = QMessageBox::warning(
        m_dialogParent,
        tr("从基线恢复"),
        tr("当前全部业务数据将被替换；系统会先自动保存一份重置前快照。是否继续？"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;
    const OperationResult result = m_dataStore->restoreBackup();
    if (!result) {
        QMessageBox::critical(m_dialogParent, tr("恢复失败"), result.error);
        return;
    }
    QMessageBox::information(m_dialogParent,
                             tr("恢复完成"),
                             tr("已恢复基线；原数据快照位于：\n%1")
                                 .arg(m_dataStore->lastSafetyBackupPath()));
}

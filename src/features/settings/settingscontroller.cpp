#include "features/settings/settingscontroller.h"

#include "data/datastore.h"

#include <QDesktopServices>
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
{
    connect(openDirectoryButton, &QPushButton::clicked, this, [this]() { openDataDirectory(); });
    connect(saveButton, &QPushButton::clicked, this, [this]() { saveNow(); });
    connect(reloadButton, &QPushButton::clicked, this, [this]() { reloadData(); });
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

#ifndef DATASTORE_H
#define DATASTORE_H

#include "common/result.h"
#include "domain/entities.h"

#include <QDateTime>
#include <QObject>

#include <memory>

class IDataRepository;

class DataStore final : public QObject
{
    Q_OBJECT

public:
    enum class LoadState
    {
        NotLoaded,
        Ready,
        Error
    };
    Q_ENUM(LoadState)

    explicit DataStore(std::unique_ptr<IDataRepository> repository, QObject *parent = nullptr);
    ~DataStore() override;

    OperationResult initialize();
    OperationResult reload();
    OperationResult save();
    OperationResult commit(domain::AppData candidate);
    OperationResult ensureBackup();
    OperationResult createBackup();
    OperationResult restoreBackup();

    const domain::AppData &data() const;
    QString dataFilePath() const;
    QString dataDirectory() const;
    QString backupFilePath() const;
    QString lastSafetyBackupPath() const;
    QDateTime lastSavedAt() const;
    LoadState loadState() const;
    QString errorMessage() const;
    bool isWritable() const;
    OperationResult login(const QString &username, const QString &password);
    OperationResult registerUser(const QString &username, const QString &password);
    void logout();
    QString currentUserId() const { return m_currentUserId; }
    QString usernameFor(const QString &id) const;
    bool isAdmin() const;
    bool canAccessOwner(const QString &ownerId) const;
    bool canAccessTicket(const QString &ticketId) const;

signals:
    void dataChanged();
    void statusChanged();

private:
    OperationResult loadFromRepository(bool createWhenMissing);
    void setError(const QString &error);
    void setReady(const domain::AppData &data);

    std::unique_ptr<IDataRepository> m_repository;
    domain::AppData m_data;
    LoadState m_loadState = LoadState::NotLoaded;
    QString m_errorMessage;
    QDateTime m_lastSavedAt;
    QString m_lastSafetyBackupPath;
    bool m_writable = false;
    QString m_currentUserId;
};

#endif // DATASTORE_H

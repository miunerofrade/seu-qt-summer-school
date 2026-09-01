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

    const domain::AppData &data() const;
    QString dataFilePath() const;
    QString dataDirectory() const;
    QDateTime lastSavedAt() const;
    LoadState loadState() const;
    QString errorMessage() const;
    bool isWritable() const;

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
    bool m_writable = false;
};

#endif // DATASTORE_H

#include "data/datastore.h"

#include "data/idatarepository.h"

#include <QFileInfo>

DataStore::DataStore(std::unique_ptr<IDataRepository> repository, QObject *parent)
    : QObject(parent)
    , m_repository(std::move(repository))
{
}

DataStore::~DataStore() = default;

OperationResult DataStore::initialize()
{
    return loadFromRepository(true);
}

OperationResult DataStore::reload()
{
    return loadFromRepository(false);
}

OperationResult DataStore::save()
{
    if (!m_writable)
        return OperationResult::failure(m_errorMessage.isEmpty()
                                            ? tr("数据仓库当前不可写。")
                                            : m_errorMessage);

    const OperationResult result = m_repository->save(m_data);
    if (!result) {
        m_errorMessage = result.error;
        emit statusChanged();
        return result;
    }

    m_lastSavedAt = QDateTime::currentDateTime();
    m_errorMessage.clear();
    emit statusChanged();
    return result;
}

OperationResult DataStore::commit(domain::AppData candidate)
{
    if (!m_writable)
        return OperationResult::failure(m_errorMessage.isEmpty()
                                            ? tr("数据仓库当前不可写。")
                                            : m_errorMessage);

    candidate.schemaVersion = domain::CurrentSchemaVersion;
    const OperationResult result = m_repository->save(candidate);
    if (!result) {
        m_errorMessage = result.error;
        emit statusChanged();
        return result;
    }

    m_data = std::move(candidate);
    m_lastSavedAt = QDateTime::currentDateTime();
    m_errorMessage.clear();
    emit dataChanged();
    emit statusChanged();
    return result;
}

const domain::AppData &DataStore::data() const
{
    return m_data;
}

QString DataStore::dataFilePath() const
{
    return m_repository->dataFilePath();
}

QString DataStore::dataDirectory() const
{
    return QFileInfo(dataFilePath()).absolutePath();
}

QDateTime DataStore::lastSavedAt() const
{
    return m_lastSavedAt;
}

DataStore::LoadState DataStore::loadState() const
{
    return m_loadState;
}

QString DataStore::errorMessage() const
{
    return m_errorMessage;
}

bool DataStore::isWritable() const
{
    return m_writable;
}

OperationResult DataStore::loadFromRepository(bool createWhenMissing)
{
    const LoadResult loaded = m_repository->load();
    if (loaded.fileMissing) {
        if (!createWhenMissing) {
            setError(tr("数据文件不存在：%1").arg(dataFilePath()));
            return OperationResult::failure(m_errorMessage);
        }

        const domain::AppData demoData = domain::createDemoData();
        const OperationResult saved = m_repository->save(demoData);
        if (!saved) {
            setError(saved.error);
            return saved;
        }
        setReady(demoData);
        m_lastSavedAt = QDateTime::currentDateTime();
        emit dataChanged();
        emit statusChanged();
        return OperationResult::ok();
    }

    if (!loaded.success) {
        setError(loaded.error);
        return OperationResult::failure(loaded.error);
    }

    setReady(loaded.data);
    m_lastSavedAt = QFileInfo(dataFilePath()).lastModified();
    emit dataChanged();
    emit statusChanged();
    return OperationResult::ok();
}

void DataStore::setError(const QString &error)
{
    m_loadState = LoadState::Error;
    m_errorMessage = error;
    m_writable = false;
    emit statusChanged();
}

void DataStore::setReady(const domain::AppData &data)
{
    m_data = data;
    m_loadState = LoadState::Ready;
    m_errorMessage.clear();
    m_writable = true;
}

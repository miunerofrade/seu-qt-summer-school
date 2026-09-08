#include "data/datastore.h"

#include "data/idatarepository.h"
#include "data/jsonrepository.h"

#include <QDir>
#include <QFileInfo>
#include <QUuid>

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

OperationResult DataStore::ensureBackup()
{
    const QString path = backupFilePath();
    if (QFileInfo::exists(path)) {
        const LoadResult loaded = JsonRepository(path).load();
        if (!loaded.success)
            return OperationResult::failure(tr("备份不可用：%1").arg(loaded.error));
        return OperationResult::ok();
    }
    return createBackup();
}

OperationResult DataStore::createBackup()
{
    if (m_loadState != LoadState::Ready)
        return OperationResult::failure(tr("业务数据尚未成功加载，无法创建备份。"));

    const OperationResult result = JsonRepository(backupFilePath()).save(m_data);
    if (result)
        emit statusChanged();
    return result;
}

OperationResult DataStore::restoreBackup()
{
    const LoadResult backup = JsonRepository(backupFilePath()).load();
    if (backup.fileMissing)
        return OperationResult::failure(tr("尚未创建备份。"));
    if (!backup.success)
        return OperationResult::failure(tr("无法读取备份时间：%1").arg(backup.error));

    const QFileInfo dataFile(dataFilePath());
    const QString historyDirectory = QDir(dataFile.absolutePath()).filePath(QStringLiteral("backups"));
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"));
    const QString safetyPath = QDir(historyDirectory).filePath(
        QStringLiteral("%1.before-reset-%2.json").arg(dataFile.completeBaseName(), timestamp));
    const OperationResult safetyResult = JsonRepository(safetyPath).save(m_data);
    if (!safetyResult)
        return OperationResult::failure(tr("重置前快照创建失败，未修改当前数据：%1").arg(safetyResult.error));

    domain::AppData restored = backup.data;
    restored.users = m_data.users;
    const OperationResult result = commit(std::move(restored));
    if (!result)
        return result;
    m_lastSafetyBackupPath = safetyPath;
    return OperationResult::ok();
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

QString DataStore::backupFilePath() const
{
    const QFileInfo fileInfo(dataFilePath());
    return QDir(fileInfo.absolutePath())
        .filePath(QStringLiteral("%1.backup.json").arg(fileInfo.completeBaseName()));
}

QString DataStore::lastSafetyBackupPath() const
{
    return m_lastSafetyBackupPath;
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

    domain::AppData readyData = loaded.data;
    if (readyData.schemaVersion != domain::CurrentSchemaVersion) {
        readyData.schemaVersion = domain::CurrentSchemaVersion;
        const OperationResult migrated = m_repository->save(readyData);
        if (!migrated) {
            setError(tr("升级本地数据失败：%1").arg(migrated.error));
            return migrated;
        }
    }
    setReady(readyData);
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

OperationResult DataStore::login(const QString &username, const QString &password)
{
    logout();
    if (m_loadState != LoadState::Ready)
        return OperationResult::failure(tr("数据尚未加载。"));
    for (const auto &user : m_data.users) {
        if (user.username.compare(username.trimmed(), Qt::CaseInsensitive) == 0
            && user.password == password) {
            m_currentUserId = user.id;
            emit dataChanged();
            return OperationResult::ok();
        }
    }
    return OperationResult::failure(tr("账号或密码错误。"));
}

OperationResult DataStore::registerUser(const QString &username, const QString &password)
{
    const QString name = username.trimmed();
    if (name.isEmpty() || password.isEmpty())
        return OperationResult::failure(tr("账号和密码不能为空。"));
    for (const auto &user : m_data.users) {
        if (user.username.compare(name, Qt::CaseInsensitive) == 0)
            return OperationResult::failure(tr("该账号已存在。"));
    }
    auto candidate = m_data;
    candidate.users.append({QUuid::createUuid().toString(QUuid::WithoutBraces),
                            name, password, QStringLiteral("user")});
    return commit(std::move(candidate));
}

void DataStore::logout()
{
    m_currentUserId.clear();
    emit dataChanged();
}

QString DataStore::usernameFor(const QString &id) const
{
    for (const auto &user : m_data.users)
        if (user.id == id)
            return user.username;
    return id;
}

bool DataStore::isAdmin() const
{
    for (const auto &user : m_data.users)
        if (user.id == m_currentUserId)
            return user.role == QStringLiteral("admin");
    return false;
}

bool DataStore::canAccessOwner(const QString &ownerId) const
{
    return !m_currentUserId.isEmpty() && (isAdmin() || ownerId == m_currentUserId);
}

bool DataStore::canAccessTicket(const QString &ticketId) const
{
    for (const auto &order : m_data.orders)
        if (order.ticketIds.contains(ticketId) && canAccessOwner(order.ownerUserId))
            return true;
    return false;
}

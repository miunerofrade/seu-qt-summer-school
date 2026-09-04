#include "features/query/querycontroller.h"

#include "data/datastore.h"
#include "features/query/trainqueryfilterproxymodel.h"
#include "features/query/trainquerymodel.h"
#include "features/query/seattypecombodelegate.h"
#include "features/query/railwayqueryservice.h"
#include "features/query/queryservice.h"

#include <QAbstractItemView>
#include <QAbstractListModel>
#include <QCollator>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDateEdit>
#include <QEvent>
#include <QElapsedTimer>
#include <QDebug>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableView>
#include <QTimer>

#include <algorithm>

// 两个车站选择器共享一个只读模型。替换其数据只会发出一次重置通知，
// 不会产生数千次逐行插入通知。
class StationListModel final : public QAbstractListModel
{
public:
    explicit StationListModel(QObject *parent) : QAbstractListModel(parent) {}
    int rowCount(const QModelIndex &parent = {}) const override
    { return parent.isValid() ? 0 : m_stations.size(); }
    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= m_stations.size())
            return {};
        const auto &station = m_stations.at(index.row());
        if (role == Qt::UserRole) return station.code;
        if (role == Qt::DisplayRole || role == Qt::EditRole) return station.name;
        return {};
    }
    void replace(QVector<RailwayStation> stations)
    {
        beginResetModel();
        m_stations = std::move(stations);
        endResetModel();
    }
private:
    QVector<RailwayStation> m_stations;
};

namespace {
QString selectedStationCode(const QComboBox *combo)
{
    if (!combo)
        return {};
    const QString text = combo->currentText().trimmed();
    // 优先使用实际选中项的数据。按名称查找存在歧义，因为本地演示数据和 12306
    // 可能包含显示名称相同但代码不同的车站。
    const int currentIndex = combo->currentIndex();
    if (currentIndex >= 0 && combo->itemText(currentIndex).trimmed() == text)
        return combo->itemData(currentIndex).toString();

    const int nameIndex = combo->findText(text, Qt::MatchFixedString);
    if (nameIndex >= 0)
        return combo->itemData(nameIndex).toString();
    const int codeIndex = combo->findData(text.toUpper(), Qt::UserRole, Qt::MatchFixedString);
    return codeIndex >= 0 ? combo->itemData(codeIndex).toString() : QString();
}
} // 命名空间

QueryController::QueryController(DataStore *dataStore,
                                 QTableView *table,
                                 QComboBox *departureStation,
                                 QComboBox *arrivalStation,
                                 QDateEdit *travelDate,
                                 QPushButton *searchButton,
                                 QComboBox *trainFilter,
                                 QComboBox *seatFilter,
                                 QCheckBox *availableOnly,
                                 QComboBox *sortCombo,
                                 QObject *parent)
    : QObject(parent)
    , m_dataStore(dataStore)
    , m_table(table)
    , m_departureStation(departureStation)
    , m_arrivalStation(arrivalStation)
    , m_travelDate(travelDate)
    , m_searchButton(searchButton)
    , m_trainFilter(trainFilter)
    , m_seatFilter(seatFilter)
    , m_availableOnly(availableOnly)
    , m_sortCombo(sortCombo)
    , m_model(new TrainQueryModel(this))
    , m_proxy(new TrainQueryFilterProxyModel(this))
    , m_railwayService(new RailwayQueryService(dataStore->dataDirectory(), this))
    , m_stationModel(new StationListModel(this))
{
    QElapsedTimer timer;
    timer.start();
    const auto report = [&timer](const char *stage) {
        const auto elapsed = timer.restart();
        if (qEnvironmentVariableIsSet("QT_SYNC_PROFILE_STARTUP"))
            qInfo("QueryController %s: %lld ms", stage, elapsed);
    };
    for (QComboBox *combo : {m_departureStation, m_arrivalStation}) {
        combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        combo->setMinimumContentsLength(10);
        combo->setEditable(true);
        combo->setInsertPolicy(QComboBox::NoInsert);
        combo->setModel(m_stationModel);
        combo->completer()->setCaseSensitivity(Qt::CaseInsensitive);
        combo->completer()->setCompletionMode(QCompleter::PopupCompletion);
        combo->completer()->setFilterMode(Qt::MatchContains);
    }
    report("station widgets");
    m_proxy->setSourceModel(m_model);
    m_model->setHiddenTrainNumbers(m_dataStore->data().hiddenTrainNumbers);
    m_proxy->sort(0, Qt::AscendingOrder);
    m_table->setModel(m_proxy);
    m_table->setEditTriggers(QAbstractItemView::CurrentChanged | QAbstractItemView::SelectedClicked);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setMouseTracking(true);
    m_table->setSortingEnabled(false);
    m_table->setItemDelegateForColumn(TrainQueryModel::SeatTypeColumn,
                                      new SeatTypeComboDelegate(m_table));
    QHeaderView *header = m_table->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->setStretchLastSection(false);
    header->setMinimumSectionSize(56);

    // 以内容为导向的最小宽度可保证每列可读；Interactive 模式允许用户调整所有宽度，
    // 而不是锁定比例。
    m_minimumColumnWidths = {
        64, 60, 90, 90, 90, 90, 72, 80, 84, 56, 72
    };
    for (int column = 0; column < m_minimumColumnWidths.size(); ++column)
        header->resizeSection(column, m_minimumColumnWidths.at(column));
    connect(header, &QHeaderView::sectionResized, this,
            [this, header](int logicalIndex, int, int newSize) {
        if (logicalIndex >= 0 && logicalIndex < m_minimumColumnWidths.size()
            && newSize < m_minimumColumnWidths.at(logicalIndex)) {
            header->resizeSection(logicalIndex, m_minimumColumnWidths.at(logicalIndex));
        }
    });
    m_table->viewport()->installEventFilter(this);
    m_table->verticalHeader()->setVisible(false);
    report("table");

    m_travelDate->setDate(QDate::currentDate());
    m_officialStations = m_railwayService->cachedStations();
    m_officialStationsReady = !m_officialStations.isEmpty();
    loadStations(m_officialStations);
    m_searchButton->setEnabled(m_officialStationsReady);
    report("station model");

    connect(searchButton, &QPushButton::clicked, this, [this]() { executeQuery(); });
    connect(m_departureStation, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
        if (!m_departureStation->currentData().toString().isEmpty()
            && m_departureStation->currentData() == m_arrivalStation->currentData()) {
            const int nextIndex = m_arrivalStation->currentIndex() == 0 ? 1 : 0;
            if (nextIndex < m_arrivalStation->count())
                m_arrivalStation->setCurrentIndex(nextIndex);
        }
    });
    connect(m_trainFilter, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() { applyFilters(); });
    connect(m_seatFilter, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() { applyFilters(); });
    connect(m_availableOnly, &QCheckBox::toggled, this, [this]() { applyFilters(); });
    connect(m_sortCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() { applyFilters(); });
    connect(m_dataStore, &DataStore::dataChanged, this, [this]() {
        m_model->setHiddenTrainNumbers(m_dataStore->data().hiddenTrainNumbers);
        loadStations(m_officialStations);
    });
    showCachedTrains();
    report("cached trains");
    if (!m_officialStationsReady || m_railwayService->stationCatalogNeedsRefresh()) {
        // 让首帧先完成渲染再初始化 HTTPS；在登录间复用新鲜目录，
        // 不必每次都启动网络会话。
        QTimer::singleShot(300, this, [this]() {
            m_railwayService->refreshStations([this](QVector<RailwayStation> refreshed,
                                                     const QString &error) {
                if (error.isEmpty() && !refreshed.isEmpty()) {
                    if (!m_officialStationsReady)
                        m_searchButton->setEnabled(true);
                    m_officialStationsReady = true;
                    m_officialStations = std::move(refreshed);
                    loadStations(m_officialStations);
                } else if (!m_officialStationsReady) {
                    // 即使在线目录失败，自定义车站仍可使用。
                    m_searchButton->setEnabled(true);
                }
            });
        });
    }
    report("station freshness check");
}

void QueryController::loadStations(const QVector<RailwayStation> &stations)
{
    QHash<QString, QString> namesByCode;
    for (const auto &station : stations)
        namesByCode.insert(station.code.toUpper(), station.name);
    for (const auto &station : m_dataStore->data().stations)
        if (station.enabled && !namesByCode.contains(station.code.toUpper()))
            namesByCode.insert(station.code.toUpper(), station.name);
    if (namesByCode == m_stationNamesByCode)
        return;
    m_stationNamesByCode = namesByCode;
    const QString departureCode = m_departureStation->currentData().toString().isEmpty()
                                      ? QStringLiteral("NKH")
                                      : m_departureStation->currentData().toString();
    const QString arrivalCode = m_arrivalStation->currentData().toString().isEmpty()
                                    ? QStringLiteral("AOH")
                                    : m_arrivalStation->currentData().toString();
    const QSignalBlocker departureBlocker(m_departureStation);
    const QSignalBlocker arrivalBlocker(m_arrivalStation);
    QVector<RailwayStation> merged;
    merged.reserve(namesByCode.size());
    for (auto it = namesByCode.cbegin(); it != namesByCode.cend(); ++it)
        merged.append({it.value(), it.key()});
    const QCollator collator;
    std::sort(merged.begin(), merged.end(), [&collator](const RailwayStation &left, const RailwayStation &right) {
        const int comparison = collator.compare(left.name, right.name);
        return comparison == 0 ? left.code < right.code : comparison < 0;
    });
    QHash<QString, int> nameCounts;
    for (const RailwayStation &station : merged)
        ++nameCounts[station.name.toCaseFolded()];
    for (RailwayStation &station : merged) {
        station.name = nameCounts.value(station.name.toCaseFolded()) > 1
                                 ? tr("%1（%2）").arg(station.name, station.code)
                                 : station.name;
    }
    m_stationModel->replace(std::move(merged));
    auto restore = [](QComboBox *combo, const QString &code, int fallback) {
        const int index = combo->findData(code);
        combo->setCurrentIndex(index >= 0 ? index : std::max(0, std::min(fallback, combo->count() - 1)));
    };
    restore(m_departureStation, departureCode, 0);
    restore(m_arrivalStation, arrivalCode, m_arrivalStation->count() > 1 ? 1 : 0);
}

bool QueryController::isOfficialStation(const QString &code) const
{
    return std::any_of(m_officialStations.cbegin(), m_officialStations.cend(), [&code](const RailwayStation &station) {
        return station.code.compare(code, Qt::CaseInsensitive) == 0;
    });
}

void QueryController::loadFilterOptions(const QVector<TrainQueryRow> &rows)
{
    const QSignalBlocker trainBlocker(m_trainFilter);
    const QSignalBlocker seatBlocker(m_seatFilter);
    const QSignalBlocker sortBlocker(m_sortCombo);
    const QString trainNumber = m_trainFilter->currentData().toString();
    const QString seatType = m_seatFilter->currentData().toString();
    m_trainFilter->clear();
    m_trainFilter->addItem(tr("全部车次"));
    QStringList trainNumbers;
    QStringList seatTypes;
    for (const TrainQueryRow &row : rows) {
        if (!trainNumbers.contains(row.trainNumber, Qt::CaseInsensitive))
            trainNumbers.push_back(row.trainNumber);
        for (const TrainSeatOption &seat : row.seats) {
            if (!seatTypes.contains(seat.seatType))
                seatTypes.push_back(seat.seatType);
        }
    }
    for (const QString &number : trainNumbers)
        m_trainFilter->addItem(number, number);
    m_seatFilter->clear();
    m_seatFilter->addItem(tr("全部席别"));
    for (const QString &type : seatTypes)
        m_seatFilter->addItem(type, type);
    const int trainIndex = m_trainFilter->findData(trainNumber);
    m_trainFilter->setCurrentIndex(trainIndex >= 0 ? trainIndex : 0);
    const int seatIndex = m_seatFilter->findData(seatType);
    m_seatFilter->setCurrentIndex(seatIndex >= 0 ? seatIndex : 0);
    m_sortCombo->clear();
    m_sortCombo->addItem(tr("发车时间"), false);
    m_sortCombo->addItem(tr("价格"), true);
}

void QueryController::executeQuery()
{
    const QString fromCode = selectedStationCode(m_departureStation);
    const QString toCode = selectedStationCode(m_arrivalStation);
    if (fromCode.isEmpty() || toCode.isEmpty()) {
        QMessageBox::warning(m_table,
                             tr("查询条件错误"),
                             tr("请输入站点全名或站码，并从候选列表中选择有效站点。"));
        return;
    }
    if (fromCode == toCode) {
        QMessageBox::warning(m_table, tr("查询条件错误"), tr("起点和终点不能相同。"));
        return;
    }

    const QDate date = m_travelDate->date();
    QVector<TrainQueryRow> localRows = QueryService(m_dataStore).query(
        {fromCode, toCode, date}, QDateTime::currentDateTime());
    if (!isOfficialStation(fromCode) || !isOfficialStation(toCode)) {
        const bool empty = localRows.isEmpty();
        showRows(std::move(localRows));
        if (empty)
            QMessageBox::information(m_table, tr("查询结果"), tr("该自定义站点暂没有可用的本地直达车次。"));
        return;
    }
    m_searchButton->setEnabled(false);
    m_searchButton->setText(tr("查询中…"));
    m_railwayService->query(fromCode, toCode, date,
                            [this, fromCode, toCode, date, localRows = std::move(localRows)](QVector<TrainQueryRow> rows,
                                                         const QString &error) mutable {
        m_searchButton->setEnabled(true);
        m_searchButton->setText(tr("查询"));
        if (!error.isEmpty()) {
            QVector<TrainQueryRow> cached = m_railwayService->cachedQuery(
                fromCode, toCode, date, QDateTime::currentDateTime());
            const bool hasCache = !cached.isEmpty();
            cached.append(localRows);
            showRows(std::move(cached));
            QMessageBox::warning(m_table,
                                 tr("在线查询失败"),
                                 hasCache
                                     ? tr("%1\n\n已显示上次保存的查询结果。").arg(error)
                                     : tr("%1\n\n没有可用的本地缓存。").arg(error));
            return;
        }
        for (TrainQueryRow &local : localRows) {
            const auto duplicate = std::find_if(rows.begin(), rows.end(), [&local](const TrainQueryRow &online) {
                return online.trainNumber.compare(local.trainNumber, Qt::CaseInsensitive) == 0
                    && online.departureStationCode == local.departureStationCode
                    && online.arrivalStationCode == local.arrivalStationCode;
            });
            if (duplicate == rows.end())
                rows.append(std::move(local));
            else
                *duplicate = std::move(local);
        }
        const bool empty = rows.isEmpty();
        showRows(std::move(rows));
        if (empty)
            QMessageBox::information(m_table, tr("查询结果"), tr("没有找到尚未发车的直达车次。"));
    });
}

void QueryController::showCachedTrains()
{
    const QDateTime now = QDateTime::currentDateTime();
    QVector<TrainQueryRow> rows = m_railwayService->cachedAvailable(now);
    rows.append(QueryService(m_dataStore).available(now));
    showRows(std::move(rows));
}

void QueryController::showRows(QVector<TrainQueryRow> rows)
{
    rows.erase(std::remove_if(rows.begin(), rows.end(), [this](const TrainQueryRow &row) {
        return m_dataStore->data().hiddenTrainNumbers.contains(row.trainNumber, Qt::CaseInsensitive);
    }), rows.end());
    loadFilterOptions(rows);
    m_model->setRows(std::move(rows));
    applyFilters();
}

void QueryController::applyFilters()
{
    m_model->selectSeatType(m_seatFilter->currentData().toString(), m_availableOnly->isChecked());
    m_proxy->setTrainNumberFilter(m_trainFilter->currentData().toString());
    m_proxy->setSeatTypeFilter(m_seatFilter->currentData().toString());
    m_proxy->setAvailableOnly(m_availableOnly->isChecked());
    m_proxy->setSortByPrice(m_sortCombo->currentData().toBool());
    QTimer::singleShot(0, this, [this]() { resizeColumnsToViewport(); });
}

bool QueryController::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_table->viewport() && event->type() == QEvent::Resize)
        QTimer::singleShot(0, this, [this]() { resizeColumnsToViewport(); });
    return QObject::eventFilter(watched, event);
}

void QueryController::resizeColumnsToViewport()
{
    if (m_resizingColumns || !m_table || !m_table->model())
        return;

    const int columnCount = m_table->model()->columnCount();
    const int availableWidth = m_table->viewport()->width();
    if (columnCount <= 0 || availableWidth <= 0
        || m_minimumColumnWidths.size() < columnCount)
        return;

    QVector<int> widths(columnCount);
    int totalWidth = 0;
    QHeaderView *header = m_table->horizontalHeader();
    for (int column = 0; column < columnCount; ++column) {
        const int contentWidth = header->sectionSizeHint(column);
        widths[column] = std::max(m_minimumColumnWidths.at(column), contentWidth);
        totalWidth += widths.at(column);
    }

    if (availableWidth > totalWidth && totalWidth > 0) {
        const int extraWidth = availableWidth - totalWidth;
        int distributed = 0;
        for (int column = 0; column < columnCount; ++column) {
            const int addition = column == columnCount - 1
                                     ? extraWidth - distributed
                                     : (extraWidth * widths.at(column)) / totalWidth;
            widths[column] += addition;
            distributed += addition;
        }
    }

    m_resizingColumns = true;
    for (int column = 0; column < columnCount; ++column)
        header->resizeSection(column, widths.at(column));
    m_resizingColumns = false;
}

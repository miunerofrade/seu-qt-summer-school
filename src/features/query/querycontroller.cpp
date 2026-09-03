#include "features/query/querycontroller.h"

#include "data/datastore.h"
#include "models/trainqueryfilterproxymodel.h"
#include "models/trainquerymodel.h"
#include "models/seattypecombodelegate.h"
#include "services/railwayqueryservice.h"
#include "services/queryservice.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDateEdit>
#include <QEvent>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableView>
#include <QTimer>

#include <algorithm>

namespace {
QString selectedStationCode(const QComboBox *combo)
{
    if (!combo)
        return {};
    const QString text = combo->currentText().trimmed();
    // Prefer the data of the actually selected item.  Looking up by name is
    // ambiguous because the local demo data and 12306 can contain stations
    // with the same display name but different codes.
    const int currentIndex = combo->currentIndex();
    if (currentIndex >= 0 && combo->itemText(currentIndex).trimmed() == text)
        return combo->itemData(currentIndex).toString();

    const int nameIndex = combo->findText(text, Qt::MatchFixedString);
    if (nameIndex >= 0)
        return combo->itemData(nameIndex).toString();
    const int codeIndex = combo->findData(text.toUpper(), Qt::UserRole, Qt::MatchFixedString);
    return codeIndex >= 0 ? combo->itemData(codeIndex).toString() : QString();
}
} // namespace

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
{
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

    // Content-oriented minimums keep every column readable while Interactive
    // mode leaves all widths user-adjustable instead of locking a ratio.
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

    m_travelDate->setDate(QDate::currentDate());
    const QVector<RailwayStation> stations = m_railwayService->cachedStations();
    m_officialStationsReady = !stations.isEmpty();
    loadStations(stations);
    m_searchButton->setEnabled(m_officialStationsReady);

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
        loadStations(m_railwayService->cachedStations());
    });
    showCachedTrains();
    m_railwayService->refreshStations([this](QVector<RailwayStation> refreshed,
                                             const QString &error) {
        if (error.isEmpty() && !refreshed.isEmpty()) {
            m_officialStationsReady = true;
            loadStations(refreshed);
        } else if (!m_officialStationsReady) {
            // The local data can still be queried when the live catalog is
            // temporarily unavailable, but do not leave the button disabled.
            m_searchButton->setEnabled(true);
        }
    });
}

void QueryController::loadStations(const QVector<RailwayStation> &stations)
{
    const QString departureCode = m_departureStation->currentData().toString().isEmpty()
                                      ? QStringLiteral("NKH")
                                      : m_departureStation->currentData().toString();
    const QString arrivalCode = m_arrivalStation->currentData().toString().isEmpty()
                                    ? QStringLiteral("AOH")
                                    : m_arrivalStation->currentData().toString();
    const QSignalBlocker departureBlocker(m_departureStation);
    const QSignalBlocker arrivalBlocker(m_arrivalStation);
    m_departureStation->clear();
    m_arrivalStation->clear();
    QVector<RailwayStation> merged = stations;
    for (const domain::Station &station : m_dataStore->data().stations) {
        const bool exists = std::any_of(merged.cbegin(), merged.cend(), [&station](const RailwayStation &item) {
            return item.code.compare(station.code, Qt::CaseInsensitive) == 0
                || item.name.compare(station.name, Qt::CaseInsensitive) == 0;
        });
        if (!exists && station.enabled)
            merged.append({station.name, station.code});
    }
    std::sort(merged.begin(), merged.end(), [](const RailwayStation &left, const RailwayStation &right) {
        return left.name.localeAwareCompare(right.name) < 0;
    });
    for (const RailwayStation &station : merged) {
        m_departureStation->addItem(station.name, station.code);
        m_arrivalStation->addItem(station.name, station.code);
    }
    m_departureStation->setEditable(true);
    m_arrivalStation->setEditable(true);
    m_departureStation->setInsertPolicy(QComboBox::NoInsert);
    m_arrivalStation->setInsertPolicy(QComboBox::NoInsert);
    for (QComboBox *combo : {m_departureStation, m_arrivalStation}) {
        combo->completer()->setCaseSensitivity(Qt::CaseInsensitive);
        combo->completer()->setCompletionMode(QCompleter::PopupCompletion);
        combo->completer()->setFilterMode(Qt::MatchContains);
    }
    auto restore = [](QComboBox *combo, const QString &code, int fallback) {
        const int index = combo->findData(code);
        combo->setCurrentIndex(index >= 0 ? index : std::max(0, std::min(fallback, combo->count() - 1)));
    };
    restore(m_departureStation, departureCode, 0);
    restore(m_arrivalStation, arrivalCode, m_arrivalStation->count() > 1 ? 1 : 0);
}

bool QueryController::isOfficialStation(const QString &code) const
{
    const QVector<RailwayStation> stations = m_railwayService->cachedStations();
    return std::any_of(stations.cbegin(), stations.cend(), [&code](const RailwayStation &station) {
        return station.code.compare(code, Qt::CaseInsensitive) == 0;
    });
}

void QueryController::loadFilterOptions(const QVector<TrainQueryRow> &rows)
{
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

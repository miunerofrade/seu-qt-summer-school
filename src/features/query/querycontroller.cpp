#include "features/query/querycontroller.h"

#include "data/datastore.h"
#include "models/trainqueryfilterproxymodel.h"
#include "models/trainquerymodel.h"
#include "services/queryservice.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QTableView>

#include <algorithm>

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
    , m_trainFilter(trainFilter)
    , m_seatFilter(seatFilter)
    , m_availableOnly(availableOnly)
    , m_sortCombo(sortCombo)
    , m_model(new TrainQueryModel(this))
    , m_proxy(new TrainQueryFilterProxyModel(this))
    , m_service(dataStore)
{
    m_proxy->setSourceModel(m_model);
    m_proxy->sort(0, Qt::AscendingOrder);
    m_table->setModel(m_proxy);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setSortingEnabled(false);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);

    m_travelDate->setDate(QDate::currentDate());
    loadStations();
    loadFilterOptions();

    connect(searchButton, &QPushButton::clicked, this, [this]() { executeQuery(); });
    connect(m_departureStation, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
        if (m_departureStation->currentData() == m_arrivalStation->currentData()) {
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
        loadStations();
        loadFilterOptions();
        executeQuery();
    });

    executeQuery();
}

void QueryController::loadStations()
{
    const QString departureCode = m_departureStation->currentData().toString().isEmpty()
                                      ? QStringLiteral("NJN")
                                      : m_departureStation->currentData().toString();
    const QString arrivalCode = m_arrivalStation->currentData().toString().isEmpty()
                                    ? QStringLiteral("SHH")
                                    : m_arrivalStation->currentData().toString();
    m_departureStation->clear();
    m_arrivalStation->clear();
    for (const domain::Station &station : m_dataStore->data().stations) {
        if (!station.enabled)
            continue;
        m_departureStation->addItem(station.name, station.code);
        m_arrivalStation->addItem(station.name, station.code);
    }
    auto restore = [](QComboBox *combo, const QString &code, int fallback) {
        const int index = combo->findData(code);
        combo->setCurrentIndex(index >= 0 ? index : std::max(0, std::min(fallback, combo->count() - 1)));
    };
    restore(m_departureStation, departureCode, 0);
    restore(m_arrivalStation, arrivalCode, m_arrivalStation->count() > 1 ? 1 : 0);
}

void QueryController::loadFilterOptions()
{
    const QString trainNumber = m_trainFilter->currentData().toString();
    const QString seatType = m_seatFilter->currentData().toString();
    m_trainFilter->clear();
    m_trainFilter->addItem(tr("全部车次"));
    QStringList trainNumbers;
    QStringList seatTypes;
    for (const domain::Train &train : m_dataStore->data().trains) {
        if (!trainNumbers.contains(train.number, Qt::CaseInsensitive))
            trainNumbers.push_back(train.number);
        for (const domain::SeatInventory &seat : train.seats) {
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
    if (m_departureStation->currentData().toString().isEmpty()
        || m_arrivalStation->currentData().toString().isEmpty()) {
        m_model->setRows({});
        return;
    }
    if (m_departureStation->currentData() == m_arrivalStation->currentData()) {
        QMessageBox::warning(m_table, tr("查询条件错误"), tr("起点和终点不能相同。"));
        return;
    }

    const TrainQueryRequest request{m_departureStation->currentData().toString(),
                                    m_arrivalStation->currentData().toString(),
                                    m_travelDate->date()};
    m_model->setRows(m_service.query(request));
    applyFilters();
    if (m_model->rowCount() == 0)
        QMessageBox::information(m_table, tr("查询结果"), tr("没有找到符合条件的直达车次。"));
}

void QueryController::applyFilters()
{
    m_proxy->setTrainNumberFilter(m_trainFilter->currentData().toString());
    m_proxy->setSeatTypeFilter(m_seatFilter->currentData().toString());
    m_proxy->setAvailableOnly(m_availableOnly->isChecked());
    m_proxy->setSortByPrice(m_sortCombo->currentData().toBool());
}

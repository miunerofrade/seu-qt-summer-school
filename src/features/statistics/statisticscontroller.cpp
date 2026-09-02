#include "features/statistics/statisticscontroller.h"

#include "data/datastore.h"
#include "models/statisticsmodel.h"
#include "widgets/flowlayout.h"
#include "widgets/seatsharechartwidget.h"
#include "widgets/trendchartwidget.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDateEdit>
#include <QHeaderView>
#include <QLabel>
#include <QLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QTableView>

namespace {
QString formatMoney(qint64 cents)
{
    return QStringLiteral("¥%1.%2").arg(cents / 100).arg(cents % 100, 2, 10, QLatin1Char('0'));
}
}

StatisticsController::StatisticsController(DataStore *dataStore, QWidget *filterCard, QWidget *quickFilterGroup,
                                           QWidget *dateFilterGroup, QWidget *trainFilterGroup, QWidget *stationFilterGroup,
                                           QWidget *seatFilterGroup, QWidget *actionGroup, QComboBox *periodCombo,
                                           QDateEdit *dateFrom, QDateEdit *dateTo, QComboBox *trainCombo,
                                           QComboBox *stationCombo, QComboBox *seatCombo, QPushButton *queryButton,
                                           QPushButton *exportButton, QLabel *soldValue, QLabel *soldCaption,
                                           QLabel *refundedValue, QLabel *refundedCaption, QLabel *revenueValue,
                                           QLabel *revenueCaption, QLabel *rateValue, QLabel *rateCaption,
                                           QWidget *trendContainer, QWidget *seatShareContainer,
                                           QTableView *trainTable, QObject *parent)
    : QObject(parent)
    , m_dataStore(dataStore), m_periodCombo(periodCombo), m_dateFrom(dateFrom), m_dateTo(dateTo)
    , m_trainCombo(trainCombo), m_stationCombo(stationCombo), m_seatCombo(seatCombo)
    , m_soldValue(soldValue), m_soldCaption(soldCaption), m_refundedValue(refundedValue)
    , m_refundedCaption(refundedCaption), m_revenueValue(revenueValue), m_revenueCaption(revenueCaption)
    , m_rateValue(rateValue), m_rateCaption(rateCaption)
    , m_trendChart(new TrendChartWidget(trendContainer))
    , m_seatShareChart(new SeatShareChartWidget(seatShareContainer)), m_model(new StatisticsModel(this))
{
    QLayout *placeholderLayout = filterCard->layout();
    while (QLayoutItem *item = placeholderLayout->takeAt(0)) delete item;
    delete placeholderLayout;
    auto *filterFlow = new FlowLayout(filterCard, 0, 12, 10);
    filterFlow->setContentsMargins(16, 12, 16, 12);
    for (QWidget *group : {quickFilterGroup, dateFilterGroup, trainFilterGroup,
                           stationFilterGroup, seatFilterGroup, actionGroup})
        filterFlow->addWidget(group);

    QLayout *trendLayout = trendContainer->layout();
    while (QLayoutItem *item = trendLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    trendLayout->setContentsMargins(0, 0, 0, 0);
    trendLayout->addWidget(m_trendChart);

    QLayout *seatShareLayout = seatShareContainer->layout();
    while (QLayoutItem *item = seatShareLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    seatShareLayout->setContentsMargins(0, 0, 0, 0);
    seatShareLayout->addWidget(m_seatShareChart);

    trainTable->setModel(m_model);
    trainTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    trainTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    trainTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    trainTable->verticalHeader()->setVisible(false);
    exportButton->setEnabled(false);

    const QDate today = QDate::currentDate();
    m_dateFrom->setDate(today);
    m_dateTo->setDate(today);
    applyQuickPeriod();
    populateOptions();
    connect(queryButton, &QPushButton::clicked, this, [this]() { refresh(); });
    connect(m_periodCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this]() { applyQuickPeriod(); });
    connect(m_dateFrom, &QDateEdit::dateChanged, this, [this]() { refresh(); });
    connect(m_dateTo, &QDateEdit::dateChanged, this, [this]() { refresh(); });
    connect(m_trainCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() { refresh(); });
    connect(m_stationCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() { refresh(); });
    connect(m_seatCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() { refresh(); });
    connect(m_dataStore, &DataStore::dataChanged, this, [this]() { populateOptions(); refresh(); });
    refresh();
}

void StatisticsController::populateOptions()
{
    const QString train = m_trainCombo->currentData().toString();
    const QString station = m_stationCombo->currentData().toString();
    const QString seat = m_seatCombo->currentData().toString();
    m_trainCombo->clear(); m_trainCombo->addItem(tr("全部车次"));
    m_stationCombo->clear(); m_stationCombo->addItem(tr("全部站点"));
    m_seatCombo->clear(); m_seatCombo->addItem(tr("全部席别"));
    QStringList trains, seats;
    for (const domain::Station &item : m_dataStore->data().stations)
        if (item.enabled) m_stationCombo->addItem(item.name, item.code);
    for (const domain::Train &item : m_dataStore->data().trains) {
        if (!item.enabled) continue;
        if (!trains.contains(item.number)) trains.append(item.number), m_trainCombo->addItem(item.number, item.number);
        for (const domain::SeatInventory &itemSeat : item.seats)
            if (!seats.contains(itemSeat.seatType)) seats.append(itemSeat.seatType), m_seatCombo->addItem(itemSeat.seatType, itemSeat.seatType);
    }
    m_trainCombo->setCurrentIndex(std::max(0, m_trainCombo->findData(train)));
    m_stationCombo->setCurrentIndex(std::max(0, m_stationCombo->findData(station)));
    m_seatCombo->setCurrentIndex(std::max(0, m_seatCombo->findData(seat)));
}

void StatisticsController::applyQuickPeriod()
{
    const QDate today = QDate::currentDate();
    if (m_periodCombo->currentIndex() == 0) m_dateFrom->setDate(today), m_dateTo->setDate(today);
    else if (m_periodCombo->currentIndex() == 1) m_dateFrom->setDate(today.addDays(-6)), m_dateTo->setDate(today);
    else if (m_periodCombo->currentIndex() == 2) m_dateFrom->setDate(QDate(today.year(), today.month(), 1)), m_dateTo->setDate(today);
}

void StatisticsController::refresh()
{
    if (m_dateFrom->date() > m_dateTo->date()) {
        m_model->setSummary({});
        m_trendChart->setPoints({});
        m_seatShareChart->setShares({});
        updateMetrics({});
        return;
    }
    const StatisticsFilter filter{m_dateFrom->date(), m_dateTo->date(), m_trainCombo->currentData().toString(),
                                  m_stationCombo->currentData().toString(), m_seatCombo->currentData().toString()};
    const StatisticsSummary summary = StatisticsService(m_dataStore).summarize(filter);
    m_model->setSummary(summary);
    m_trendChart->setPoints(summary.dailyTrend);
    m_seatShareChart->setShares(summary.seatShares);
    updateMetrics(summary);
}

void StatisticsController::updateMetrics(const StatisticsSummary &summary)
{
    const bool hasData = !summary.rows.isEmpty() || summary.soldCount > 0 || summary.refundedCount > 0;
    m_soldValue->setText(hasData ? QString::number(summary.soldCount) : QStringLiteral("--"));
    m_refundedValue->setText(hasData ? QString::number(summary.refundedCount) : QStringLiteral("--"));
    m_revenueValue->setText(hasData ? formatMoney(summary.netRevenueCents) : QStringLiteral("--"));
    m_rateValue->setText(hasData || summary.averageRemainingRate > 0.0
                             ? QStringLiteral("%1%").arg(summary.averageRemainingRate * 100.0, 0, 'f', 1)
                             : QStringLiteral("--"));
    const QString caption = hasData ? tr("%1 条明细").arg(summary.rows.size()) : tr("暂无数据");
    m_soldCaption->setText(caption); m_refundedCaption->setText(caption);
    m_revenueCaption->setText(caption); m_rateCaption->setText(caption);
}

#include "features/statistics/statisticscontroller.h"
#include "common/money.h"

#include "data/datastore.h"

#include <QLabel>

StatisticsController::StatisticsController(DataStore *dataStore, QLabel *soldValue, QLabel *refundedValue,
                                           QLabel *revenueValue,
                                           QLabel *rateValue, QObject *parent)
    : QObject(parent)
    , m_dataStore(dataStore), m_soldValue(soldValue), m_refundedValue(refundedValue)
    , m_revenueValue(revenueValue), m_rateValue(rateValue)
{
    connect(m_dataStore, &DataStore::dataChanged, this, [this]() { refresh(); });
    refresh();
}

void StatisticsController::refresh()
{
    updateMetrics(StatisticsService(m_dataStore).summarize());
}

void StatisticsController::updateMetrics(const StatisticsSummary &summary)
{
    m_soldValue->setText(QString::number(summary.soldCount));
    m_refundedValue->setText(QString::number(summary.refundedCount));
    m_revenueValue->setText(common::formatMoney(summary.netRevenueCents));
    m_rateValue->setText(QStringLiteral("%1%").arg(summary.averageRemainingRate * 100.0, 0, 'f', 1));
}

#include "widgets/seatsharechartwidget.h"

#include <QPainter>

#include <algorithm>

namespace {
const QColor ChartColors[] = {
    QColor(QStringLiteral("#0A84FF")),
    QColor(QStringLiteral("#5AC8FA")),
    QColor(QStringLiteral("#34C759")),
    QColor(QStringLiteral("#FF9F0A")),
    QColor(QStringLiteral("#AF52DE")),
};
}

SeatShareChartWidget::SeatShareChartWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(170);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void SeatShareChartWidget::setShares(QVector<SeatSharePoint> shares)
{
    m_shares = std::move(shares);
    update();
}

void SeatShareChartWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(QStringLiteral("#FAFBFC")));

    int total = 0;
    for (const SeatSharePoint &share : m_shares)
        total += share.soldCount;
    if (total <= 0) {
        painter.setPen(QColor(QStringLiteral("#8A9099")));
        painter.drawText(rect(), Qt::AlignCenter, tr("所选范围暂无席别销售记录"));
        return;
    }

    const qreal diameter = std::min<qreal>(height() - 28, width() * 0.42);
    const QRectF donutRect(14, (height() - diameter) / 2.0, diameter, diameter);
    int startAngle = 90 * 16;
    painter.setPen(Qt::NoPen);
    for (int index = 0; index < m_shares.size(); ++index) {
        const SeatSharePoint &share = m_shares.at(index);
        const int span = index == m_shares.size() - 1
                             ? -(360 * 16 - (90 * 16 - startAngle))
                             : -qRound(360.0 * 16 * share.soldCount / total);
        painter.setBrush(ChartColors[index % std::size(ChartColors)]);
        painter.drawPie(donutRect, startAngle, span);
        startAngle += span;
    }

    const qreal holeDiameter = diameter * 0.58;
    const QRectF holeRect(donutRect.center().x() - holeDiameter / 2.0,
                          donutRect.center().y() - holeDiameter / 2.0,
                          holeDiameter,
                          holeDiameter);
    painter.setBrush(QColor(QStringLiteral("#FAFBFC")));
    painter.drawEllipse(holeRect);
    painter.setPen(QColor(QStringLiteral("#1D1D1F")));
    QFont totalFont = painter.font();
    totalFont.setPointSize(13);
    totalFont.setBold(true);
    painter.setFont(totalFont);
    painter.drawText(holeRect.adjusted(0, -8, 0, 0), Qt::AlignCenter, QString::number(total));
    QFont captionFont = painter.font();
    captionFont.setPointSize(8);
    captionFont.setBold(false);
    painter.setFont(captionFont);
    painter.setPen(QColor(QStringLiteral("#8A9099")));
    painter.drawText(holeRect.adjusted(0, 18, 0, 0), Qt::AlignCenter, tr("售票"));

    const qreal legendLeft = donutRect.right() + 18;
    const qreal rowHeight = 24;
    const qreal legendTop = (height() - rowHeight * m_shares.size()) / 2.0;
    painter.setFont(QFont(painter.font().family(), 9));
    for (int index = 0; index < m_shares.size(); ++index) {
        const qreal y = legendTop + index * rowHeight;
        painter.setBrush(ChartColors[index % std::size(ChartColors)]);
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(QRectF(legendLeft, y + 6, 10, 10), 2, 2);
        painter.setPen(QColor(QStringLiteral("#515866")));
        const int percent = qRound(100.0 * m_shares.at(index).soldCount / total);
        const QString label = QStringLiteral("%1  %2%")
                                  .arg(m_shares.at(index).seatType)
                                  .arg(percent);
        painter.drawText(QRectF(legendLeft + 17, y, width() - legendLeft - 22, rowHeight),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         label);
    }
}

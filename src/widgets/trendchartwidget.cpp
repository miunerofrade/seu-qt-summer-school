#include "widgets/trendchartwidget.h"

#include <QFontMetrics>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>

namespace {
constexpr int ChartLeft = 38;
constexpr int ChartTop = 12;
constexpr int ChartRight = 12;
constexpr int ChartBottom = 28;
constexpr int GridLines = 4;
}

TrendChartWidget::TrendChartWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(170);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void TrendChartWidget::setPoints(QVector<DailyStatisticsPoint> points)
{
    m_points = std::move(points);
    update();
}

void TrendChartWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(QStringLiteral("#FAFBFC")));

    const bool hasSales = std::any_of(m_points.cbegin(), m_points.cend(), [](const DailyStatisticsPoint &point) {
        return point.soldCount > 0;
    });
    if (m_points.isEmpty() || !hasSales) {
        painter.setPen(QColor(QStringLiteral("#8A9099")));
        painter.drawText(rect(), Qt::AlignCenter, tr("所选范围暂无售票记录"));
        return;
    }

    const QRectF chartRect = rect().adjusted(ChartLeft, ChartTop, -ChartRight, -ChartBottom);
    if (chartRect.width() <= 1.0 || chartRect.height() <= 1.0)
        return;

    int maximum = 1;
    for (const DailyStatisticsPoint &point : m_points)
        maximum = std::max(maximum, point.soldCount);
    maximum = std::max(GridLines, ((maximum + GridLines - 1) / GridLines) * GridLines);

    painter.setFont(QFont(painter.font().family(), 8));
    for (int line = 0; line <= GridLines; ++line) {
        const qreal ratio = static_cast<qreal>(line) / GridLines;
        const qreal y = chartRect.bottom() - ratio * chartRect.height();
        painter.setPen(QPen(QColor(QStringLiteral("#E5E9EF")), 1));
        painter.drawLine(QPointF(chartRect.left(), y), QPointF(chartRect.right(), y));
        painter.setPen(QColor(QStringLiteral("#8A9099")));
        const QString label = QString::number(qRound(ratio * maximum));
        painter.drawText(QRectF(0, y - 8, ChartLeft - 6, 16), Qt::AlignRight | Qt::AlignVCenter, label);
    }

    const auto pointPosition = [&](int index) {
        const qreal xRatio = m_points.size() == 1
                                 ? 0.5
                                 : static_cast<qreal>(index) / (m_points.size() - 1);
        const qreal yRatio = static_cast<qreal>(m_points.at(index).soldCount) / maximum;
        return QPointF(chartRect.left() + xRatio * chartRect.width(),
                       chartRect.bottom() - yRatio * chartRect.height());
    };

    QPainterPath linePath;
    if (m_points.size() == 1) {
        const qreal y = pointPosition(0).y();
        linePath.moveTo(chartRect.left(), y);
        linePath.lineTo(chartRect.right(), y);
    } else {
        linePath.moveTo(pointPosition(0));
        for (int index = 1; index < m_points.size(); ++index)
            linePath.lineTo(pointPosition(index));
    }

    QPainterPath areaPath = linePath;
    areaPath.lineTo(chartRect.right(), chartRect.bottom());
    areaPath.lineTo(chartRect.left(), chartRect.bottom());
    areaPath.closeSubpath();
    QLinearGradient fill(chartRect.topLeft(), chartRect.bottomLeft());
    fill.setColorAt(0.0, QColor(10, 132, 255, 72));
    fill.setColorAt(1.0, QColor(10, 132, 255, 4));
    painter.fillPath(areaPath, fill);

    painter.setPen(QPen(QColor(QStringLiteral("#0A84FF")), 2.25, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(linePath);
    if (m_points.size() <= 31) {
        painter.setBrush(QColor(QStringLiteral("#FFFFFF")));
        painter.setPen(QPen(QColor(QStringLiteral("#0A84FF")), 2));
        for (int index = 0; index < m_points.size(); ++index)
            painter.drawEllipse(pointPosition(index), 3.0, 3.0);
    }

    const int labelCount = std::min(4, static_cast<int>(m_points.size()));
    painter.setPen(QColor(QStringLiteral("#8A9099")));
    for (int labelIndex = 0; labelIndex < labelCount; ++labelIndex) {
        const int index = labelCount == 1
                              ? 0
                              : qRound(static_cast<qreal>(labelIndex) * (m_points.size() - 1)
                                       / (labelCount - 1));
        const QPointF point = pointPosition(index);
        const QString label = m_points.at(index).date.toString(QStringLiteral("MM-dd"));
        painter.drawText(QRectF(point.x() - 32, chartRect.bottom() + 7, 64, 16),
                         Qt::AlignHCenter | Qt::AlignTop,
                         label);
    }
}

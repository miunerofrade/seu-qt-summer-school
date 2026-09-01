#include "widgets/dateedit.h"

#include <QCalendarWidget>
#include <QColor>
#include <QDate>
#include <QLineEdit>
#include <QTextCharFormat>

DateEdit::DateEdit(QWidget *parent)
    : QDateEdit(parent)
{
    setDisplayFormat(QStringLiteral("MM-dd"));
    setCalendarPopup(true);
    // 由 QDateEdit 绘制唯一的外层边框，内部编辑框不再重复绘制边框。
    if (lineEdit()) {
        lineEdit()->setFrame(false);
        lineEdit()->setStyleSheet(QStringLiteral("border: none; border-radius: 0; background: transparent;"));
    }
    // 界面只显示月-日，但控件内部仍保存完整日期（包括年份），默认使用当前日期。
    setDate(QDate::currentDate());
    setFocusPolicy(Qt::NoFocus);
    setMinimumHeight(28);
    setMaximumHeight(28);

    calendarWidget()->setStyleSheet(QStringLiteral(R"(
QCalendarWidget {
    background-color: #FFFFFF;
}

QCalendarWidget QWidget {
    background-color: #FFFFFF;
}

QCalendarWidget QToolButton {
    background-color: transparent;
    border: none;
    border-radius: 0;
    margin: 3px;
    padding: 4px 16px 4px 6px;
    color: #31363B;
}

QCalendarWidget QToolButton:hover {
    background-color: #F2F6FA;
}

QCalendarWidget QToolButton:pressed,
QCalendarWidget QToolButton:checked {
    background-color: rgba(10, 132, 255, 40);
}

QCalendarWidget QToolButton#qt_calendar_prevmonth {
    border: none;
    image: url(:/icons/chevron-left.svg);
    qproperty-iconSize: 0px 0px;
    min-width: 28px;
    min-height: 28px;
    padding: 4px;
}

QCalendarWidget QToolButton#qt_calendar_nextmonth {
    border: none;
    image: url(:/icons/chevron-right.svg);
    qproperty-iconSize: 0px 0px;
    min-width: 28px;
    min-height: 28px;
    padding: 4px;
}

QCalendarWidget QToolButton::menu-indicator {
    image: url(:/icons/chevron-down.svg);
    width: 12px;
    height: 8px;
    subcontrol-origin: padding;
    subcontrol-position: center right;
    right: 6px;
}

QCalendarWidget QAbstractItemView:enabled {
    background-color: #FFFFFF;
    border: none;
    outline: none;
    selection-background-color: #0A84FF;
    selection-color: #FFFFFF;
}

QCalendarWidget QAbstractItemView::item:hover {
    background-color: #E8F3FF;
    color: #0A66C2;
}

QCalendarWidget QAbstractItemView::item:selected:hover {
    background-color: #0071E3;
    color: #FFFFFF;
}

QCalendarWidget QSpinBox {
    background-color: transparent;
    border: none;
    border-radius: 0;
    min-width: 56px;
    padding: 2px 6px;
    color: #31363B;
}

QCalendarWidget QSpinBox QLineEdit {
    background-color: transparent;
    border: none;
    border-radius: 0;
}
)"));

    connect(calendarWidget(), &QCalendarWidget::currentPageChanged,
            this, &DateEdit::updateAdjacentMonthFormats);
    updateAdjacentMonthFormats(calendarWidget()->yearShown(), calendarWidget()->monthShown());
}

void DateEdit::updateAdjacentMonthFormats(int year, int month)
{
    auto *calendar = calendarWidget();

    // 翻页前先恢复上一页设置过的日期，否则它们进入本月后仍会保持灰色。
    for (const QDate &date : m_adjacentMonthDates) {
        calendar->setDateTextFormat(date, QTextCharFormat());
    }
    m_adjacentMonthDates.clear();

    const QDate firstOfMonth(year, month, 1);
    const int offset = (firstOfMonth.dayOfWeek()
                        - static_cast<int>(calendar->firstDayOfWeek()) + 7) % 7;
    const QDate firstVisibleDate = firstOfMonth.addDays(-offset);

    QTextCharFormat adjacentMonthFormat;
    adjacentMonthFormat.setForeground(QColor(QStringLiteral("#A8ADB4")));

    // QCalendarWidget 固定显示六行日期；把不属于当前页月份的日期弱化。
    for (int dayOffset = 0; dayOffset < 42; ++dayOffset) {
        const QDate date = firstVisibleDate.addDays(dayOffset);
        if (date.year() == year && date.month() == month) {
            continue;
        }
        calendar->setDateTextFormat(date, adjacentMonthFormat);
        m_adjacentMonthDates.append(date);
    }
}

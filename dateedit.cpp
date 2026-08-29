#include "dateedit.h"

#include <QCalendarWidget>
#include <QDate>

DateEdit::DateEdit(QWidget *parent)
    : QDateEdit(parent)
{
    setDisplayFormat(QStringLiteral("MM-dd"));
    setCalendarPopup(true);
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
    color: #31363B;
}

QCalendarWidget QToolButton {
    background-color: transparent;
    border: 1px solid #E5E7EB;
    border-radius: 4px;
    margin: 3px;
    padding: 4px 16px 4px 6px;
    color: #31363B;
}

QCalendarWidget QToolButton:hover {
    border-color: #0A84FF;
}

QCalendarWidget QToolButton:pressed,
QCalendarWidget QToolButton:checked {
    background-color: rgba(10, 132, 255, 40);
    border-color: #0A84FF;
}

QCalendarWidget QToolButton#qt_calendar_prevmonth {
    image: url(:/icons/chevron-left.svg);
    qproperty-iconSize: 0px 0px;
    min-width: 28px;
    min-height: 28px;
    padding: 4px;
}

QCalendarWidget QToolButton#qt_calendar_nextmonth {
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
    color: #31363B;
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
    background-color: #FFFFFF;
    border: 1px solid #BAB9B8;
    border-radius: 4px;
    min-width: 56px;
    padding: 2px 6px;
    color: #31363B;
}
)"));
}

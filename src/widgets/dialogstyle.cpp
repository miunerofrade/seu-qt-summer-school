#include "widgets/dialogstyle.h"

#include <QDialog>
#include <QDateEdit>
#include <QLineEdit>
#include <QPushButton>

void applyStandardDialogStyle(QDialog *dialog)
{
    dialog->setStyleSheet(QStringLiteral(R"(
QPushButton {
    background-color: #FFFFFF;
    border: 1px solid #D8DDE6;
    border-radius: 7px;
    color: #2F3033;
    padding: 3px 14px;
}
QPushButton:hover {
    background-color: #F5F8FC;
    border-color: #AEB8C6;
}
QPushButton:pressed {
    background-color: #E8EEF6;
    border-color: #8FA1B5;
}
QPushButton:default {
    background-color: #0A84FF;
    border-color: #0A84FF;
    color: #FFFFFF;
}
QPushButton:disabled {
    background-color: #F4F5F7;
    border-color: #E3E5E8;
    color: #A1A1A6;
}
QLineEdit, QDateEdit, QTableView {
    border: 1px solid #D8DDE6;
    border-radius: 6px;
}
QTableView {
    outline: none;
    selection-background-color: #0A84FF;
    selection-color: #FFFFFF;
}
QTableView::item:selected,
QTableView::item:selected:active,
QTableView::item:selected:!active {
    background-color: #0A84FF;
    border: none;
    border-radius: 0px;
    color: #FFFFFF;
}
QTableView::item:hover {
    background-color: transparent;
    border: none;
    border-radius: 0px;
}
QTableView::item:selected:hover {
    background-color: #0A84FF;
    color: #FFFFFF;
}
QTableView QHeaderView {
    background-color: #FFFFFF;
    border: none;
    border-top-left-radius: 5px;
    border-top-right-radius: 5px;
}
QTableView QHeaderView::section {
    background-color: #FFFFFF;
    border: none;
    border-bottom: 1px solid #E5E7EB;
    color: #6E6E73;
    padding: 8px;
}
QTableView QHeaderView::section:first {
    border-top-left-radius: 5px;
}
QTableView QHeaderView::section:last {
    border-top-right-radius: 5px;
}
QTableView QHeaderView::section:only-one {
    border-top-left-radius: 5px;
    border-top-right-radius: 5px;
}
QTableView QHeaderView::down-arrow {
    image: url(:/icons/chevron-down.svg);
    width: 10px;
    height: 10px;
    subcontrol-origin: padding;
    subcontrol-position: right center;
    right: 6px;
}
QTableView QHeaderView::up-arrow {
    image: url(:/icons/chevron-up.svg);
    width: 10px;
    height: 10px;
    subcontrol-origin: padding;
    subcontrol-position: right center;
    right: 6px;
}
QLineEdit, QDateEdit {
    min-height: 26px;
}
QDateEdit QLineEdit {
    border: none;
    border-radius: 0;
}
)"));

    const auto buttons = dialog->findChildren<QPushButton *>();
    for (QPushButton *button : buttons) {
        button->setFixedHeight(28);
        button->setMinimumWidth(88);
    }
    const auto dateEdits = dialog->findChildren<QDateEdit *>();
    for (QDateEdit *dateEdit : dateEdits) {
        if (auto *editor = dateEdit->findChild<QLineEdit *>()) {
            editor->setFrame(false);
            editor->setStyleSheet(QStringLiteral("border: none; border-radius: 0; background: transparent;"));
        }
    }
}

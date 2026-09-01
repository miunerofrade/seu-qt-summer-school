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

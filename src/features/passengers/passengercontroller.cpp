#include "features/passengers/passengercontroller.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>

namespace {
QStringList editPassenger(QWidget *parent, const QStringList &values = {})
{
    QDialog dialog(parent);
    dialog.setWindowTitle(values.isEmpty() ? QObject::tr("新增乘车人") : QObject::tr("编辑乘车人"));
    dialog.resize(460, 340);

    auto *layout = new QFormLayout(&dialog);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setHorizontalSpacing(18);
    layout->setVerticalSpacing(14);
    auto *nameEdit = new QLineEdit(values.value(0), &dialog);
    auto *idTypeEdit = new QComboBox(&dialog);
    idTypeEdit->addItems({QObject::tr("身份证"), QObject::tr("护照"), QObject::tr("港澳通行证")});
    idTypeEdit->setCurrentText(values.value(1, QObject::tr("身份证")));
    auto *idNumberEdit = new QLineEdit(values.value(2), &dialog);
    auto *typeEdit = new QComboBox(&dialog);
    typeEdit->addItems({QObject::tr("成人"), QObject::tr("儿童"), QObject::tr("学生")});
    typeEdit->setCurrentText(values.value(4, QObject::tr("成人")));

    nameEdit->setMinimumWidth(260);
    idTypeEdit->setMinimumWidth(260);
    idNumberEdit->setMinimumWidth(260);
    typeEdit->setMinimumWidth(260);

    layout->addRow(QObject::tr("姓名"), nameEdit);
    layout->addRow(QObject::tr("证件类型"), idTypeEdit);
    layout->addRow(QObject::tr("证件号码"), idNumberEdit);
    layout->addRow(QObject::tr("旅客类型"), typeEdit);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->setCenterButtons(true);
    buttons->button(QDialogButtonBox::Ok)->setText(QObject::tr("确定"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QObject::tr("取消"));
    layout->addRow(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
        return {};

    return {nameEdit->text(), idTypeEdit->currentText(), idNumberEdit->text(), typeEdit->currentText()};
}
}

PassengerController::PassengerController(QWidget *dialogParent,
                                         QTableWidget *table,
                                         QLineEdit *searchEdit,
                                         QPushButton *addButton,
                                         QPushButton *editButton,
                                         QPushButton *deleteButton,
                                         QPushButton *searchButton,
                                         QObject *parent)
    : QObject(parent)
    , m_dialogParent(dialogParent)
    , m_table(table)
    , m_searchEdit(searchEdit)
{
    m_searchEdit->addAction(QIcon(":/icons/nav-search.svg"), QLineEdit::LeadingPosition);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    m_table->setColumnCount(4);

    connect(addButton, &QPushButton::clicked, this, [this]() { addPassenger(); });
    connect(editButton, &QPushButton::clicked, this, [this]() { editSelectedPassenger(); });
    connect(deleteButton, &QPushButton::clicked, this, [this]() { deleteSelectedPassenger(); });
    connect(searchButton, &QPushButton::clicked, this, [this]() { filterPassengers(); });
    connect(m_searchEdit, &QLineEdit::returnPressed, this, [this]() { filterPassengers(); });
}

void PassengerController::addPassenger()
{
    const QStringList values = editPassenger(m_dialogParent);
    if (values.isEmpty())
        return;

    const int row = m_table->rowCount();
    m_table->insertRow(row);
    for (int column = 0; column < values.size(); ++column)
        m_table->setItem(row, column, new QTableWidgetItem(values.at(column)));
}

void PassengerController::editSelectedPassenger()
{
    const int row = m_table->currentRow();
    if (row < 0)
        return;

    QStringList values;
    for (int column = 0; column < m_table->columnCount(); ++column)
        values.append(m_table->item(row, column)->text());

    const QStringList editedValues = editPassenger(m_dialogParent, values);
    if (editedValues.isEmpty())
        return;

    for (int column = 0; column < editedValues.size(); ++column)
        m_table->item(row, column)->setText(editedValues.at(column));
}

void PassengerController::deleteSelectedPassenger()
{
    const int row = m_table->currentRow();
    if (row >= 0)
        m_table->removeRow(row);
}

void PassengerController::filterPassengers()
{
    const QString keyword = m_searchEdit->text().trimmed();
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const bool matches = keyword.isEmpty()
            || m_table->item(row, 0)->text().contains(keyword, Qt::CaseInsensitive)
            || m_table->item(row, 2)->text().contains(keyword, Qt::CaseInsensitive);
        m_table->setRowHidden(row, !matches);
    }
}

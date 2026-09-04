#include "features/passengers/passengercontroller.h"

#include "data/datastore.h"
#include "features/passengers/passengerfilterproxymodel.h"
#include "features/passengers/passengertablemodel.h"
#include "features/passengers/passengerservice.h"
#include "widgets/dialogstyle.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QIcon>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableView>

namespace {
struct PassengerFormData
{
    QString name;
    QString documentType;
    QString documentNumber;
};

bool editPassenger(QWidget *parent,
                   const domain::Passenger *current,
                   PassengerFormData *formData)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(current ? QObject::tr("编辑乘车人") : QObject::tr("新增乘车人"));
    dialog.resize(460, 340);

    auto *layout = new QFormLayout(&dialog);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setHorizontalSpacing(18);
    layout->setVerticalSpacing(14);
    auto *nameEdit = new QLineEdit(current ? current->name : QString(), &dialog);
    auto *documentTypeEdit = new QComboBox(&dialog);
    documentTypeEdit->addItems({QObject::tr("身份证"), QObject::tr("护照"), QObject::tr("港澳通行证")});
    if (current)
        documentTypeEdit->setCurrentText(current->documentType);
    auto *documentNumberEdit = new QLineEdit(current ? current->documentNumber : QString(), &dialog);
    auto *validationError = new QLabel(&dialog);
    validationError->setStyleSheet(QStringLiteral("color: #d32f2f;"));
    validationError->setVisible(false);

    nameEdit->setMinimumWidth(260);
    nameEdit->setMaxLength(20);
    documentTypeEdit->setMinimumWidth(260);
    documentNumberEdit->setMinimumWidth(260);
    documentNumberEdit->setMaxLength(documentTypeEdit->currentText() == QObject::tr("身份证") ? 18 : 30);
    layout->addRow(QObject::tr("姓名"), nameEdit);
    layout->addRow(QObject::tr("证件类型"), documentTypeEdit);
    layout->addRow(QObject::tr("证件号码"), documentNumberEdit);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->setCenterButtons(true);
    buttons->button(QDialogButtonBox::Ok)->setText(QObject::tr("确定"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QObject::tr("取消"));
    layout->addRow(QString(), validationError);
    layout->addRow(buttons);
    auto validateOnSubmit = [=]() {
        const QString name = nameEdit->text().trimmed();
        const bool nameValid = !name.isEmpty() && name.size() >= 2 && name.size() <= 20;

        const bool isIdentity = documentTypeEdit->currentText() == QObject::tr("身份证");
        const QString number = documentNumberEdit->text().trimmed();
        const bool numberValid = !number.isEmpty()
            && (!isIdentity || (number.size() == 18 && PassengerService::isValidChineseIdCard(number)));
        QStringList errors;
        if (!nameValid)
            errors << QObject::tr("不正确的姓名");
        if (!numberValid)
            errors << QObject::tr("不正确的身份证号码");
        validationError->setText(errors.join(QStringLiteral("\n")));
        validationError->setVisible(!errors.isEmpty());
        return errors.isEmpty();
    };
    QObject::connect(documentTypeEdit, &QComboBox::currentTextChanged, &dialog, [=]() {
        const bool isIdentity = documentTypeEdit->currentText() == QObject::tr("身份证");
        documentNumberEdit->setMaxLength(isIdentity ? 18 : 30);
    });
    QObject::connect(buttons->button(QDialogButtonBox::Ok), &QPushButton::clicked, &dialog, [&dialog, validateOnSubmit]() {
        if (validateOnSubmit())
            dialog.accept();
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    applyStandardDialogStyle(&dialog);

    if (dialog.exec() != QDialog::Accepted)
        return false;
    formData->name = nameEdit->text();
    formData->documentType = documentTypeEdit->currentText();
    formData->documentNumber = documentNumberEdit->text();
    return true;
}
} // namespace

PassengerController::PassengerController(DataStore *dataStore,
                                         QWidget *dialogParent,
                                         QTableView *table,
                                         QLineEdit *searchEdit,
                                         QPushButton *addButton,
                                         QPushButton *editButton,
                                         QPushButton *deleteButton,
                                         QPushButton *searchButton,
                                         QObject *parent)
    : QObject(parent)
    , m_service(dataStore)
    , m_dialogParent(dialogParent)
    , m_table(table)
    , m_searchEdit(searchEdit)
    , m_model(new PassengerTableModel(dataStore, this))
    , m_proxy(new PassengerFilterProxyModel(this))
{
    m_proxy->setSourceModel(m_model);
    m_table->setModel(m_proxy);
    m_table->setColumnHidden(PassengerTableModel::OwnerColumn, !dataStore->isAdmin());
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSortingEnabled(true);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    m_searchEdit->addAction(QIcon(":/icons/nav-search.svg"), QLineEdit::LeadingPosition);

    connect(addButton, &QPushButton::clicked, this, [this]() { addPassenger(); });
    connect(editButton, &QPushButton::clicked, this, [this]() { editSelectedPassenger(); });
    connect(deleteButton, &QPushButton::clicked, this, [this]() { deleteSelectedPassenger(); });
    connect(searchButton, &QPushButton::clicked, this, [this]() { filterPassengers(); });
    connect(m_searchEdit, &QLineEdit::returnPressed, this, [this]() { filterPassengers(); });
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this]() { filterPassengers(); });
    connect(m_table, &QTableView::doubleClicked, this, [this](const QModelIndex &) {
        editSelectedPassenger();
    });
}

void PassengerController::addPassenger()
{
    PassengerFormData formData;
    if (!editPassenger(m_dialogParent, nullptr, &formData))
        return;
    const OperationResult result = m_service.addPassenger(
        formData.name, formData.documentType, formData.documentNumber);
    if (!result)
        QMessageBox::warning(m_dialogParent, tr("无法新增乘车人"), result.error);
}

void PassengerController::editSelectedPassenger()
{
    const QModelIndex proxyIndex = m_table->currentIndex();
    if (!proxyIndex.isValid()) {
        QMessageBox::information(m_dialogParent, tr("编辑乘车人"), tr("请先选择一名乘车人。"));
        return;
    }
    const QModelIndex sourceIndex = m_proxy->mapToSource(proxyIndex);
    const domain::Passenger *passenger = m_model->passengerAt(sourceIndex.row());
    if (!passenger)
        return;
    const domain::Passenger snapshot = *passenger;

    PassengerFormData formData;
    if (!editPassenger(m_dialogParent, &snapshot, &formData))
        return;
    const OperationResult result = m_service.updatePassenger(
        snapshot.id, formData.name, formData.documentType, formData.documentNumber);
    if (!result)
        QMessageBox::warning(m_dialogParent, tr("无法修改乘车人"), result.error);
}

void PassengerController::deleteSelectedPassenger()
{
    const QString passengerId = selectedPassengerId();
    if (passengerId.isEmpty()) {
        QMessageBox::information(m_dialogParent, tr("删除乘车人"), tr("请先选择一名乘车人。"));
        return;
    }
    if (QMessageBox::question(m_dialogParent,
                              tr("删除乘车人"),
                              tr("确定删除所选乘车人吗？"),
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No)
        != QMessageBox::Yes) {
        return;
    }
    const OperationResult result = m_service.removePassenger(passengerId);
    if (!result)
        QMessageBox::warning(m_dialogParent, tr("无法删除乘车人"), result.error);
}

void PassengerController::filterPassengers()
{
    m_proxy->setKeyword(m_searchEdit->text());
}

QString PassengerController::selectedPassengerId() const
{
    const QModelIndex proxyIndex = m_table->currentIndex();
    if (!proxyIndex.isValid())
        return {};
    return m_model->data(m_proxy->mapToSource(proxyIndex), Qt::UserRole).toString();
}

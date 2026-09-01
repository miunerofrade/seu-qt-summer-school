#include "features/admin/managementdialogs.h"

#include "data/datastore.h"
#include "services/adminservice.h"
#include "widgets/dialogstyle.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTableView>
#include <QVBoxLayout>

#include <algorithm>
#include <iterator>

namespace {
void configureTable(QTableView *table)
{
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setAlternatingRowColors(true);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

bool reportResult(QWidget *parent, const OperationResult &result, const QString &successText)
{
    if (!result) {
        QMessageBox::warning(parent, QObject::tr("操作失败"), result.error);
        return false;
    }
    if (!successText.isEmpty())
        QMessageBox::information(parent, QObject::tr("操作完成"), successText);
    return true;
}

const domain::Station *stationByCode(const domain::AppData &data, const QString &code)
{
    const auto station = std::find_if(data.stations.cbegin(), data.stations.cend(), [&code](const domain::Station &item) {
        return item.code == code;
    });
    return station == data.stations.cend() ? nullptr : &*station;
}

const domain::Train *trainByNumber(const domain::AppData &data, const QString &number)
{
    const auto train = std::find_if(data.trains.cbegin(), data.trains.cend(), [&number](const domain::Train &item) {
        return item.number == number;
    });
    return train == data.trains.cend() ? nullptr : &*train;
}

bool editStationForm(QWidget *parent, const domain::Station *current, domain::Station *output)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(current ? QObject::tr("编辑车站") : QObject::tr("新增车站"));
    auto *layout = new QFormLayout(&dialog);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setVerticalSpacing(12);
    auto *codeEdit = new QLineEdit(current ? current->code : QString(), &dialog);
    auto *nameEdit = new QLineEdit(current ? current->name : QString(), &dialog);
    auto *cityEdit = new QLineEdit(current ? current->city : QString(), &dialog);
    auto *enabledCheck = new QCheckBox(QObject::tr("允许用于新车次"), &dialog);
    enabledCheck->setChecked(!current || current->enabled);
    codeEdit->setEnabled(!current);
    layout->addRow(QObject::tr("站码"), codeEdit);
    layout->addRow(QObject::tr("车站名称"), nameEdit);
    layout->addRow(QObject::tr("所在城市"), cityEdit);
    layout->addRow(QObject::tr("状态"), enabledCheck);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Save)->setText(QObject::tr("保存"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QObject::tr("取消"));
    layout->addRow(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted)
        return false;
    *output = {codeEdit->text(), nameEdit->text(), cityEdit->text(), enabledCheck->isChecked()};
    return true;
}

bool editTrainForm(QWidget *parent, const domain::Train *current, domain::Train *output)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(current ? QObject::tr("编辑车次") : QObject::tr("新增车次"));
    auto *layout = new QFormLayout(&dialog);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setVerticalSpacing(12);
    auto *numberEdit = new QLineEdit(current ? current->number : QString(), &dialog);
    auto *dateEdit = new QDateEdit(current ? current->serviceDate : QDate::currentDate(), &dialog);
    auto *enabledCheck = new QCheckBox(QObject::tr("启用车次"), &dialog);
    auto *saleOpenCheck = new QCheckBox(QObject::tr("允许售票"), &dialog);
    numberEdit->setEnabled(!current);
    dateEdit->setCalendarPopup(true);
    dateEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    enabledCheck->setChecked(!current || current->enabled);
    saleOpenCheck->setChecked(!current || current->saleOpen);
    layout->addRow(QObject::tr("车次号"), numberEdit);
    layout->addRow(QObject::tr("运行日期"), dateEdit);
    layout->addRow(QObject::tr("使用状态"), enabledCheck);
    layout->addRow(QObject::tr("售票状态"), saleOpenCheck);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Save)->setText(QObject::tr("保存"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QObject::tr("取消"));
    layout->addRow(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted)
        return false;
    output->number = numberEdit->text();
    output->serviceDate = dateEdit->date();
    output->enabled = enabledCheck->isChecked();
    output->saleOpen = saleOpenCheck->isChecked();
    if (current) {
        output->stops = current->stops;
        output->seats = current->seats;
    }
    return true;
}

class StationManagementDialog final : public QDialog
{
public:
    StationManagementDialog(DataStore *dataStore, QWidget *parent)
        : QDialog(parent)
        , m_dataStore(dataStore)
        , m_service(dataStore)
        , m_table(new QTableView(this))
        , m_model(new QStandardItemModel(this))
    {
        setWindowTitle(tr("车站管理"));
        resize(720, 440);
        auto *layout = new QVBoxLayout(this);
        auto *hint = new QLabel(tr("站码创建后不可修改；已被引用的车站请改为停用。"), this);
        hint->setWordWrap(true);
        layout->addWidget(hint);
        configureTable(m_table);
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->setModel(m_model);
        layout->addWidget(m_table);
        auto *actions = new QHBoxLayout;
        auto *addButton = new QPushButton(tr("新增"), this);
        auto *editButton = new QPushButton(tr("编辑"), this);
        auto *deleteButton = new QPushButton(tr("删除"), this);
        auto *closeButton = new QPushButton(tr("关闭"), this);
        actions->addWidget(addButton);
        actions->addWidget(editButton);
        actions->addWidget(deleteButton);
        actions->addStretch();
        actions->addWidget(closeButton);
        layout->addLayout(actions);
        connect(addButton, &QPushButton::clicked, this, [this]() { addStation(); });
        connect(editButton, &QPushButton::clicked, this, [this]() { editStation(); });
        connect(deleteButton, &QPushButton::clicked, this, [this]() { deleteStation(); });
        connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
        connect(m_dataStore, &DataStore::dataChanged, this, [this]() { refresh(); });
        refresh();
        applyStandardDialogStyle(this);
    }

private:
    QString selectedCode() const
    {
        const QModelIndex index = m_table->currentIndex();
        return index.isValid() ? m_model->item(index.row(), 0)->data(Qt::UserRole).toString() : QString();
    }

    void refresh()
    {
        m_model->clear();
        m_model->setHorizontalHeaderLabels({tr("站码"), tr("车站名称"), tr("城市"), tr("状态")});
        for (const domain::Station &station : m_dataStore->data().stations) {
            QList<QStandardItem *> row{new QStandardItem(station.code),
                                      new QStandardItem(station.name),
                                      new QStandardItem(station.city),
                                      new QStandardItem(station.enabled ? tr("启用") : tr("停用"))};
            row.first()->setData(station.code, Qt::UserRole);
            m_model->appendRow(row);
        }
    }

    void addStation()
    {
        domain::Station station;
        if (editStationForm(this, nullptr, &station))
            reportResult(this, m_service.addStation(station), {});
    }

    void editStation()
    {
        const QString code = selectedCode();
        const domain::Station *current = stationByCode(m_dataStore->data(), code);
        if (!current) {
            QMessageBox::information(this, tr("编辑车站"), tr("请先选择一个车站。"));
            return;
        }
        const domain::Station snapshot = *current;
        domain::Station station;
        if (editStationForm(this, &snapshot, &station))
            reportResult(this,
                         m_service.updateStation(code, station.name, station.city, station.enabled),
                         {});
    }

    void deleteStation()
    {
        const QString code = selectedCode();
        if (code.isEmpty()) {
            QMessageBox::information(this, tr("删除车站"), tr("请先选择一个车站。"));
            return;
        }
        if (QMessageBox::question(this, tr("删除车站"), tr("确定删除车站 %1 吗？").arg(code),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
            == QMessageBox::Yes) {
            reportResult(this, m_service.removeStation(code), {});
        }
    }

    DataStore *m_dataStore;
    AdminService m_service;
    QTableView *m_table;
    QStandardItemModel *m_model;
};

class TrainManagementDialog final : public QDialog
{
public:
    TrainManagementDialog(DataStore *dataStore, QWidget *parent)
        : QDialog(parent)
        , m_dataStore(dataStore)
        , m_service(dataStore)
        , m_table(new QTableView(this))
        , m_model(new QStandardItemModel(this))
    {
        setWindowTitle(tr("车次管理"));
        resize(780, 440);
        auto *layout = new QVBoxLayout(this);
        layout->addWidget(new QLabel(tr("车次号创建后不可修改；经停站、票价和余票由对应管理入口配置。"), this));
        configureTable(m_table);
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->setModel(m_model);
        layout->addWidget(m_table);
        auto *actions = new QHBoxLayout;
        auto *addButton = new QPushButton(tr("新增"), this);
        auto *editButton = new QPushButton(tr("编辑"), this);
        auto *deleteButton = new QPushButton(tr("删除"), this);
        auto *closeButton = new QPushButton(tr("关闭"), this);
        actions->addWidget(addButton);
        actions->addWidget(editButton);
        actions->addWidget(deleteButton);
        actions->addStretch();
        actions->addWidget(closeButton);
        layout->addLayout(actions);
        connect(addButton, &QPushButton::clicked, this, [this]() { addTrain(); });
        connect(editButton, &QPushButton::clicked, this, [this]() { editTrain(); });
        connect(deleteButton, &QPushButton::clicked, this, [this]() { deleteTrain(); });
        connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
        connect(m_dataStore, &DataStore::dataChanged, this, [this]() { refresh(); });
        refresh();
        applyStandardDialogStyle(this);
    }

private:
    QString selectedNumber() const
    {
        const QModelIndex index = m_table->currentIndex();
        return index.isValid() ? m_model->item(index.row(), 0)->data(Qt::UserRole).toString() : QString();
    }

    void refresh()
    {
        m_model->clear();
        m_model->setHorizontalHeaderLabels(
            {tr("车次"), tr("运行日期"), tr("经停站"), tr("席别"), tr("状态"), tr("售票")});
        for (const domain::Train &train : m_dataStore->data().trains) {
            QList<QStandardItem *> row{new QStandardItem(train.number),
                                      new QStandardItem(train.serviceDate.toString(QStringLiteral("yyyy-MM-dd"))),
                                      new QStandardItem(QString::number(train.stops.size())),
                                      new QStandardItem(QString::number(train.seats.size())),
                                      new QStandardItem(train.enabled ? tr("启用") : tr("停用")),
                                      new QStandardItem(train.saleOpen ? tr("开售") : tr("停售"))};
            row.first()->setData(train.number, Qt::UserRole);
            m_model->appendRow(row);
        }
    }

    void addTrain()
    {
        domain::Train train;
        if (editTrainForm(this, nullptr, &train))
            reportResult(this, m_service.addTrain(train), {});
    }

    void editTrain()
    {
        const QString number = selectedNumber();
        const domain::Train *current = trainByNumber(m_dataStore->data(), number);
        if (!current) {
            QMessageBox::information(this, tr("编辑车次"), tr("请先选择一个车次。"));
            return;
        }
        const domain::Train snapshot = *current;
        domain::Train train;
        if (editTrainForm(this, &snapshot, &train))
            reportResult(this,
                         m_service.updateTrain(number, train.serviceDate, train.enabled, train.saleOpen),
                         {});
    }

    void deleteTrain()
    {
        const QString number = selectedNumber();
        if (number.isEmpty()) {
            QMessageBox::information(this, tr("删除车次"), tr("请先选择一个车次。"));
            return;
        }
        if (QMessageBox::question(this, tr("删除车次"), tr("确定删除车次 %1 吗？").arg(number),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
            == QMessageBox::Yes) {
            reportResult(this, m_service.removeTrain(number), {});
        }
    }

    DataStore *m_dataStore;
    AdminService m_service;
    QTableView *m_table;
    QStandardItemModel *m_model;
};

class ScheduleManagementDialog final : public QDialog
{
public:
    ScheduleManagementDialog(DataStore *dataStore, QWidget *parent)
        : QDialog(parent)
        , m_dataStore(dataStore)
        , m_service(dataStore)
        , m_trainCombo(new QComboBox(this))
        , m_table(new QTableView(this))
        , m_model(new QStandardItemModel(this))
    {
        setWindowTitle(tr("时刻与经停站管理"));
        resize(820, 520);
        auto *layout = new QVBoxLayout(this);
        auto *selector = new QHBoxLayout;
        selector->addWidget(new QLabel(tr("车次"), this));
        selector->addWidget(m_trainCombo);
        selector->addStretch();
        layout->addLayout(selector);
        layout->addWidget(new QLabel(tr("时间格式为 HH:mm；首站到达、末站出发可以留空。保存时将清空原席别配置。"), this));
        configureTable(m_table);
        m_table->setModel(m_model);
        layout->addWidget(m_table);
        auto *actions = new QHBoxLayout;
        auto *addButton = new QPushButton(tr("新增站"), this);
        auto *removeButton = new QPushButton(tr("删除站"), this);
        auto *upButton = new QPushButton(tr("上移"), this);
        auto *downButton = new QPushButton(tr("下移"), this);
        auto *saveButton = new QPushButton(tr("保存"), this);
        auto *closeButton = new QPushButton(tr("关闭"), this);
        actions->addWidget(addButton);
        actions->addWidget(removeButton);
        actions->addWidget(upButton);
        actions->addWidget(downButton);
        actions->addStretch();
        actions->addWidget(saveButton);
        actions->addWidget(closeButton);
        layout->addLayout(actions);
        connect(m_trainCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() { refreshRows(); });
        connect(addButton, &QPushButton::clicked, this, [this]() { addStop(); });
        connect(removeButton, &QPushButton::clicked, this, [this]() { removeStop(); });
        connect(upButton, &QPushButton::clicked, this, [this]() { moveStop(-1); });
        connect(downButton, &QPushButton::clicked, this, [this]() { moveStop(1); });
        connect(saveButton, &QPushButton::clicked, this, [this]() { save(); });
        connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
        refreshTrains();
        applyStandardDialogStyle(this);
    }

private:
    QString currentTrainNumber() const { return m_trainCombo->currentData().toString(); }

    void refreshTrains()
    {
        m_trainCombo->clear();
        for (const domain::Train &train : m_dataStore->data().trains)
            m_trainCombo->addItem(train.number, train.number);
        refreshRows();
    }

    void refreshRows()
    {
        m_model->clear();
        m_model->setHorizontalHeaderLabels({tr("车站"), tr("到达"), tr("出发"), tr("跨日")});
        const domain::Train *train = trainByNumber(m_dataStore->data(), currentTrainNumber());
        if (!train)
            return;
        for (const domain::TrainStop &stop : train->stops) {
            m_model->appendRow({new QStandardItem(stop.stationCode),
                                new QStandardItem(stop.arrivalTime.isValid()
                                                      ? stop.arrivalTime.toString(QStringLiteral("HH:mm"))
                                                      : QString()),
                                new QStandardItem(stop.departureTime.isValid()
                                                      ? stop.departureTime.toString(QStringLiteral("HH:mm"))
                                                      : QString()),
                                new QStandardItem(QString::number(stop.dayOffset))});
        }
    }

    void addStop()
    {
        QStringList stations;
        for (const domain::Station &station : m_dataStore->data().stations) {
            if (station.enabled)
                stations.append(station.code + QStringLiteral(" - ") + station.name);
        }
        bool ok = false;
        const QString selected = QInputDialog::getItem(this, tr("新增经停站"), tr("车站"), stations, 0, false, &ok);
        if (!ok || selected.isEmpty())
            return;
        const QString code = selected.section(QStringLiteral(" - "), 0, 0);
        m_model->appendRow({new QStandardItem(code),
                            new QStandardItem(QStringLiteral("00:00")),
                            new QStandardItem(QStringLiteral("00:00")),
                            new QStandardItem(QStringLiteral("0"))});
    }

    void removeStop()
    {
        const QModelIndex index = m_table->currentIndex();
        if (index.isValid())
            m_model->removeRow(index.row());
    }

    void moveStop(int offset)
    {
        const QModelIndex index = m_table->currentIndex();
        const int target = index.row() + offset;
        if (!index.isValid() || target < 0 || target >= m_model->rowCount())
            return;
        const QList<QStandardItem *> row = m_model->takeRow(index.row());
        m_model->insertRow(target, row);
        m_table->selectRow(target);
    }

    void save()
    {
        QVector<domain::TrainStop> stops;
        for (int row = 0; row < m_model->rowCount(); ++row) {
            domain::TrainStop stop;
            stop.stationCode = m_model->item(row, 0)->text().trimmed().toUpper();
            stop.sequence = row;
            const QString arrival = m_model->item(row, 1)->text().trimmed();
            const QString departure = m_model->item(row, 2)->text().trimmed();
            stop.arrivalTime = arrival.isEmpty() ? QTime() : QTime::fromString(arrival, QStringLiteral("HH:mm"));
            stop.departureTime = departure.isEmpty() ? QTime() : QTime::fromString(departure, QStringLiteral("HH:mm"));
            bool dayOk = false;
            stop.dayOffset = m_model->item(row, 3)->text().toInt(&dayOk);
            if ((!arrival.isEmpty() && !stop.arrivalTime.isValid())
                || (!departure.isEmpty() && !stop.departureTime.isValid()) || !dayOk) {
                QMessageBox::warning(this, tr("格式错误"), tr("第 %1 行的时间或跨日格式无效。").arg(row + 1));
                return;
            }
            stops.append(stop);
        }
        if (reportResult(this,
                         m_service.replaceStops(currentTrainNumber(), stops),
                         tr("时刻与经停站已保存，请重新配置席别、票价和余票。"))) {
            refreshRows();
        }
    }

    DataStore *m_dataStore;
    AdminService m_service;
    QComboBox *m_trainCombo;
    QTableView *m_table;
    QStandardItemModel *m_model;
};

class SeatManagementDialog final : public QDialog
{
public:
    SeatManagementDialog(DataStore *dataStore, QWidget *parent)
        : QDialog(parent)
        , m_dataStore(dataStore)
        , m_service(dataStore)
        , m_trainCombo(new QComboBox(this))
        , m_table(new QTableView(this))
        , m_model(new QStandardItemModel(this))
    {
        setWindowTitle(tr("席别、票价与余票管理"));
        resize(900, 520);
        auto *layout = new QVBoxLayout(this);
        auto *selector = new QHBoxLayout;
        selector->addWidget(new QLabel(tr("车次"), this));
        selector->addWidget(m_trainCombo);
        selector->addStretch();
        layout->addLayout(selector);
        layout->addWidget(new QLabel(tr("每个席别按相邻区间配置票价、总票额和余票。票价单位为元。"), this));
        configureTable(m_table);
        m_table->setModel(m_model);
        layout->addWidget(m_table);
        auto *actions = new QHBoxLayout;
        auto *addButton = new QPushButton(tr("新增席别"), this);
        auto *removeButton = new QPushButton(tr("删除席别"), this);
        auto *saveButton = new QPushButton(tr("保存"), this);
        auto *closeButton = new QPushButton(tr("关闭"), this);
        actions->addWidget(addButton);
        actions->addWidget(removeButton);
        actions->addStretch();
        actions->addWidget(saveButton);
        actions->addWidget(closeButton);
        layout->addLayout(actions);
        connect(m_trainCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() { refreshRows(); });
        connect(addButton, &QPushButton::clicked, this, [this]() { addSeat(); });
        connect(removeButton, &QPushButton::clicked, this, [this]() { removeSeat(); });
        connect(saveButton, &QPushButton::clicked, this, [this]() { save(); });
        connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
        refreshTrains();
        applyStandardDialogStyle(this);
    }

private:
    QString currentTrainNumber() const { return m_trainCombo->currentData().toString(); }

    void refreshTrains()
    {
        m_trainCombo->clear();
        for (const domain::Train &train : m_dataStore->data().trains)
            m_trainCombo->addItem(train.number, train.number);
        refreshRows();
    }

    void appendSegmentRow(const QString &seatType,
                          const QString &segmentName,
                          const domain::SegmentInventory &segment)
    {
        auto *segmentItem = new QStandardItem(segmentName);
        segmentItem->setEditable(false);
        m_model->appendRow({new QStandardItem(seatType),
                            segmentItem,
                            new QStandardItem(QString::number(segment.priceCents / 100.0, 'f', 2)),
                            new QStandardItem(QString::number(segment.totalSeats)),
                            new QStandardItem(QString::number(segment.remainingSeats))});
    }

    void refreshRows()
    {
        m_model->clear();
        m_model->setHorizontalHeaderLabels({tr("席别"), tr("区间"), tr("票价（元）"), tr("总票额"), tr("余票")});
        const domain::Train *train = trainByNumber(m_dataStore->data(), currentTrainNumber());
        if (!train)
            return;
        for (const domain::SeatInventory &seat : train->seats) {
            for (int index = 0; index < seat.segments.size(); ++index) {
                const QString segmentName = train->stops.at(index).stationCode + QStringLiteral(" → ")
                    + train->stops.at(index + 1).stationCode;
                appendSegmentRow(seat.seatType, segmentName, seat.segments.at(index));
            }
        }
    }

    void addSeat()
    {
        const domain::Train *train = trainByNumber(m_dataStore->data(), currentTrainNumber());
        if (!train || train->stops.size() < 2) {
            QMessageBox::warning(this, tr("新增席别"), tr("请先为车次配置至少两个经停站。"));
            return;
        }
        bool ok = false;
        const QString seatType = QInputDialog::getText(this, tr("新增席别"), tr("席别名称"), QLineEdit::Normal, {}, &ok).trimmed();
        if (!ok || seatType.isEmpty())
            return;
        for (int index = 0; index < train->stops.size() - 1; ++index) {
            const QString segmentName = train->stops.at(index).stationCode + QStringLiteral(" → ")
                + train->stops.at(index + 1).stationCode;
            appendSegmentRow(seatType, segmentName, {});
        }
    }

    void removeSeat()
    {
        const QModelIndex index = m_table->currentIndex();
        if (!index.isValid())
            return;
        const QString seatType = m_model->item(index.row(), 0)->text();
        for (int row = m_model->rowCount() - 1; row >= 0; --row) {
            if (m_model->item(row, 0)->text().compare(seatType, Qt::CaseInsensitive) == 0)
                m_model->removeRow(row);
        }
    }

    void save()
    {
        const domain::Train *train = trainByNumber(m_dataStore->data(), currentTrainNumber());
        if (!train)
            return;
        QVector<domain::SeatInventory> seats;
        for (int row = 0; row < m_model->rowCount(); ++row) {
            const QString seatType = m_model->item(row, 0)->text().trimmed();
            auto seat = std::find_if(seats.begin(), seats.end(), [&seatType](const domain::SeatInventory &item) {
                return item.seatType.compare(seatType, Qt::CaseInsensitive) == 0;
            });
            if (seat == seats.end()) {
                seats.append({seatType, {}});
                seat = std::prev(seats.end());
            }
            bool priceOk = false;
            bool totalOk = false;
            bool remainingOk = false;
            const double price = m_model->item(row, 2)->text().toDouble(&priceOk);
            const int total = m_model->item(row, 3)->text().toInt(&totalOk);
            const int remaining = m_model->item(row, 4)->text().toInt(&remainingOk);
            if (!priceOk || !totalOk || !remainingOk || price < 0) {
                QMessageBox::warning(this, tr("格式错误"), tr("第 %1 行的票价或票额格式无效。").arg(row + 1));
                return;
            }
            seat->segments.append({qRound64(price * 100.0), total, remaining});
        }
        if (reportResult(this, m_service.replaceSeats(currentTrainNumber(), seats), tr("席别、票价和余票已保存。")))
            refreshRows();
    }

    DataStore *m_dataStore;
    AdminService m_service;
    QComboBox *m_trainCombo;
    QTableView *m_table;
    QStandardItemModel *m_model;
};
} // namespace

void showStationManagementDialog(QWidget *parent, DataStore *dataStore)
{
    StationManagementDialog(dataStore, parent).exec();
}

void showTrainManagementDialog(QWidget *parent, DataStore *dataStore)
{
    TrainManagementDialog(dataStore, parent).exec();
}

void showScheduleManagementDialog(QWidget *parent, DataStore *dataStore)
{
    ScheduleManagementDialog(dataStore, parent).exec();
}

void showSeatManagementDialog(QWidget *parent, DataStore *dataStore)
{
    SeatManagementDialog(dataStore, parent).exec();
}

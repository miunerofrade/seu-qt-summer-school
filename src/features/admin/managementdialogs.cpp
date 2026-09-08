#include "features/admin/managementdialogs.h"

#include "data/datastore.h"
#include "features/admin/adminservice.h"
#include "features/query/railwayqueryservice.h"
#include "widgets/dialogstyle.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTimeEdit>
#include <QVBoxLayout>

#include <algorithm>
#include <iterator>

namespace {
class MoneyDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &,
                          const QModelIndex &) const override
    {
        auto *editor = new QDoubleSpinBox(parent);
        editor->setRange(0, 1000000);
        editor->setDecimals(2);
        editor->setSingleStep(10);
        return editor;
    }
    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        static_cast<QDoubleSpinBox *>(editor)->setValue(index.data(Qt::EditRole).toDouble());
    }
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override
    {
        model->setData(index, static_cast<QDoubleSpinBox *>(editor)->value(), Qt::EditRole);
    }
};

class CountDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &,
                          const QModelIndex &) const override
    {
        auto *editor = new QSpinBox(parent);
        editor->setRange(0, 100000);
        return editor;
    }
    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        static_cast<QSpinBox *>(editor)->setValue(index.data(Qt::EditRole).toInt());
    }
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override
    {
        model->setData(index, static_cast<QSpinBox *>(editor)->value(), Qt::EditRole);
    }
};

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

QString stationDisplayName(const domain::AppData &data, const QString &code)
{
    const domain::Station *station = stationByCode(data, code);
    return station && !station->name.isEmpty() ? station->name : code;
}

const domain::Train *trainByNumber(const domain::AppData &data, const QString &number)
{
    const auto train = std::find_if(data.trains.cbegin(), data.trains.cend(), [&number](const domain::Train &item) {
        return item.number == number;
    });
    return train == data.trains.cend() ? nullptr : &*train;
}

QVector<RailwayStation> availableStations(DataStore *dataStore)
{
    RailwayQueryService service(dataStore->dataDirectory());
    QVector<RailwayStation> stations = service.cachedStations();
    for (const domain::Station &station : dataStore->data().stations) {
        if (!station.enabled)
            continue;
        const bool duplicate = std::any_of(stations.cbegin(), stations.cend(), [&station](const RailwayStation &item) {
            return item.code.compare(station.code, Qt::CaseInsensitive) == 0;
        });
        if (!duplicate)
            stations.append({station.name, station.code});
    }
    std::sort(stations.begin(), stations.end(), [](const RailwayStation &left, const RailwayStation &right) {
        return left.name.localeAwareCompare(right.name) < 0;
    });
    return stations;
}

bool isOfficialStation(DataStore *dataStore, const QString &code)
{
    const QVector<RailwayStation> stations = RailwayQueryService(dataStore->dataDirectory()).cachedStations();
    return std::any_of(stations.cbegin(), stations.cend(), [&code](const RailwayStation &station) {
        return station.code.compare(code, Qt::CaseInsensitive) == 0;
    });
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

bool addTrainForm(QWidget *parent, const QVector<RailwayStation> &stations, domain::Train *output)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(QObject::tr("新增车次"));
    dialog.resize(520, 460);
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(12);
    auto *note = new QLabel(QObject::tr(
        "手工车次按每日重复运行保存。由于无法估计实际运行线路，相邻经停站时刻统一按 1 小时间隔生成；可稍后在“时刻 / 经停站”中调整。"),
                            &dialog);
    note->setWordWrap(true);
    layout->addWidget(note);
    auto *form = new QFormLayout;
    auto *numberEdit = new QLineEdit(&dialog);
    auto *startTimeEdit = new QTimeEdit(QTime(8, 0), &dialog);
    startTimeEdit->setDisplayFormat(QStringLiteral("HH:mm"));
    form->addRow(QObject::tr("车次号"), numberEdit);
    form->addRow(QObject::tr("首站发车"), startTimeEdit);
    layout->addLayout(form);

    auto *stops = new QListWidget(&dialog);
    layout->addWidget(new QLabel(QObject::tr("经停站（按运行顺序，至少两个）"), &dialog));
    layout->addWidget(stops);
    auto *stopActions = new QHBoxLayout;
    auto *addStopButton = new QPushButton(QObject::tr("增加经停站"), &dialog);
    auto *removeStopButton = new QPushButton(QObject::tr("移除"), &dialog);
    auto *upButton = new QPushButton(QObject::tr("上移"), &dialog);
    auto *downButton = new QPushButton(QObject::tr("下移"), &dialog);
    stopActions->addWidget(addStopButton);
    stopActions->addWidget(removeStopButton);
    stopActions->addWidget(upButton);
    stopActions->addWidget(downButton);
    stopActions->addStretch();
    layout->addLayout(stopActions);

    QObject::connect(addStopButton, &QPushButton::clicked, &dialog, [&dialog, &stations, stops]() {
        QStringList choices;
        for (const RailwayStation &station : stations)
            choices.append(station.code + QStringLiteral(" - ") + station.name);
        bool ok = false;
        const QString selected = QInputDialog::getItem(
            &dialog, QObject::tr("增加经停站"), QObject::tr("车站"), choices, 0, false, &ok);
        if (!ok || selected.isEmpty())
            return;
        const QString code = selected.section(QStringLiteral(" - "), 0, 0);
        for (int index = 0; index < stops->count(); ++index) {
            if (stops->item(index)->data(Qt::UserRole).toString() == code) {
                QMessageBox::information(&dialog, QObject::tr("增加经停站"), QObject::tr("该车站已经在运行线路中。"));
                return;
            }
        }
        auto *item = new QListWidgetItem(selected, stops);
        item->setData(Qt::UserRole, code);
    });
    QObject::connect(removeStopButton, &QPushButton::clicked, &dialog, [stops]() {
        delete stops->takeItem(stops->currentRow());
    });
    const auto moveStop = [stops](int offset) {
        const int row = stops->currentRow();
        const int target = row + offset;
        if (row < 0 || target < 0 || target >= stops->count())
            return;
        stops->insertItem(target, stops->takeItem(row));
        stops->setCurrentRow(target);
    };
    QObject::connect(upButton, &QPushButton::clicked, &dialog, [moveStop]() { moveStop(-1); });
    QObject::connect(downButton, &QPushButton::clicked, &dialog, [moveStop]() { moveStop(1); });

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Save)->setText(QObject::tr("保存"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QObject::tr("取消"));
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted)
        return false;
    output->number = numberEdit->text();
    const int startMinute = startTimeEdit->time().hour() * 60 + startTimeEdit->time().minute();
    for (int index = 0; index < stops->count(); ++index) {
        const int absoluteMinute = startMinute + index * 60;
        const QTime time = QTime(0, 0).addSecs((absoluteMinute % (24 * 60)) * 60);
        domain::TrainStop stop;
        stop.stationCode = stops->item(index)->data(Qt::UserRole).toString();
        stop.sequence = index;
        stop.dayOffset = absoluteMinute / (24 * 60);
        stop.arrivalTime = index == 0 ? QTime() : time;
        stop.departureTime = index == stops->count() - 1 ? QTime() : time;
        output->stops.append(stop);
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
        auto *hint = new QLabel(tr("仅维护自定义站点。不可与 12306 名称或站码重复。"), this);
        hint->setWordWrap(true);
        hint->setStyleSheet(QStringLiteral("color: #8E8E93; font-size: 12px;"));
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
            if (isOfficialStation(m_dataStore, station.code))
                continue;
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
        , m_railwayService(new RailwayQueryService(dataStore->dataDirectory(), this))
        , m_table(new QTableView(this))
        , m_model(new QStandardItemModel(this))
        , m_toggleButton(new QPushButton(this))
    {
        setWindowTitle(tr("车次管理"));
        resize(780, 440);
        auto *layout = new QVBoxLayout(this);
        configureTable(m_table);
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->setModel(m_model);
        layout->addWidget(m_table);
        auto *actions = new QHBoxLayout;
        auto *addButton = new QPushButton(tr("新增"), this);
        auto *closeButton = new QPushButton(tr("关闭"), this);
        actions->addWidget(addButton);
        actions->addWidget(m_toggleButton);
        actions->addStretch();
        actions->addWidget(closeButton);
        layout->addLayout(actions);
        connect(addButton, &QPushButton::clicked, this, [this]() { addTrain(); });
        connect(m_toggleButton, &QPushButton::clicked, this, [this]() { toggleTrainVisibility(); });
        connect(m_table->selectionModel(), &QItemSelectionModel::currentRowChanged,
                this, [this]() { updateToggleButton(); });
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

    bool selectedTrainIsHidden() const
    {
        const QModelIndex index = m_table->currentIndex();
        return index.isValid() && m_model->item(index.row(), 0)->data(Qt::UserRole + 1).toBool();
    }

    void updateToggleButton()
    {
        const bool selected = !selectedNumber().isEmpty();
        m_toggleButton->setEnabled(selected);
        m_toggleButton->setText(selected && selectedTrainIsHidden() ? tr("恢复显示") : tr("隐藏"));
    }

    void refresh()
    {
        const QString previousNumber = selectedNumber();
        m_model->clear();
        m_model->setHorizontalHeaderLabels(
            {tr("车次"), tr("状态"), tr("来源"), tr("区间"), tr("发车"), tr("到达"), tr("历时"), tr("席别")});
        QStringList visibleNumbers;
        for (const domain::Train &train : m_dataStore->data().trains) {
            if (train.railwayServiceDate.isValid())
                continue;
            const bool hidden = m_dataStore->data().hiddenTrainNumbers.contains(train.number, Qt::CaseInsensitive);
            const QString origin = train.stops.isEmpty() ? tr("未配置") : train.stops.first().stationCode;
            const QString terminal = train.stops.isEmpty() ? tr("未配置") : train.stops.last().stationCode;
            const QTime departure = train.stops.isEmpty() ? QTime() : train.stops.first().departureTime;
            int durationMinutes = 0;
            if (train.stops.size() >= 2 && departure.isValid() && train.stops.last().arrivalTime.isValid()) {
                durationMinutes = train.stops.last().dayOffset * 24 * 60
                                  + train.stops.last().arrivalTime.hour() * 60
                                  + train.stops.last().arrivalTime.minute()
                                  - departure.hour() * 60 - departure.minute();
            }
            QList<QStandardItem *> row{new QStandardItem(train.number),
                                      new QStandardItem(hidden ? tr("隐藏") : tr("显示")),
                                      new QStandardItem(tr("本地新增")),
                                      new QStandardItem(origin + QStringLiteral(" → ") + terminal),
                                      new QStandardItem(departure.isValid() ? departure.toString(QStringLiteral("HH:mm")) : tr("未配置")),
                                      new QStandardItem(train.stops.isEmpty() || !train.stops.last().arrivalTime.isValid()
                                                            ? tr("未配置")
                                                            : train.stops.last().arrivalTime.toString(QStringLiteral("HH:mm"))),
                                      new QStandardItem(durationMinutes > 0
                                                            ? tr("%1小时%2分").arg(durationMinutes / 60).arg(durationMinutes % 60, 2, 10, QLatin1Char('0'))
                                                            : tr("未配置")),
                                      new QStandardItem(QString::number(train.seats.size()))};
            row.first()->setData(train.number, Qt::UserRole);
            row.first()->setData(hidden, Qt::UserRole + 1);
            m_model->appendRow(row);
            visibleNumbers.append(train.number);
        }
        for (const TrainQueryRow &train : m_railwayService->cachedAll()) {
            if (visibleNumbers.contains(train.trainNumber, Qt::CaseInsensitive))
                continue;
            const bool hidden = m_dataStore->data().hiddenTrainNumbers.contains(train.trainNumber, Qt::CaseInsensitive);
            QList<QStandardItem *> row{
                new QStandardItem(train.trainNumber),
                new QStandardItem(hidden ? tr("隐藏") : tr("显示")),
                new QStandardItem(tr("12306缓存")),
                new QStandardItem(train.departureStationName + QStringLiteral(" → ") + train.arrivalStationName),
                new QStandardItem(train.departureTime.toString(QStringLiteral("HH:mm"))),
                new QStandardItem(train.arrivalTime.toString(QStringLiteral("HH:mm"))),
                new QStandardItem(tr("%1小时%2分").arg(train.durationMinutes / 60).arg(train.durationMinutes % 60, 2, 10, QLatin1Char('0'))),
                new QStandardItem(QString::number(train.seats.size()))};
            row.first()->setData(train.trainNumber, Qt::UserRole);
            row.first()->setData(hidden, Qt::UserRole + 1);
            m_model->appendRow(row);
            visibleNumbers.append(train.trainNumber);
        }
        for (const QString &number : m_dataStore->data().hiddenTrainNumbers) {
            if (visibleNumbers.contains(number, Qt::CaseInsensitive))
                continue;
            QList<QStandardItem *> row{
                new QStandardItem(number), new QStandardItem(tr("隐藏")),
                new QStandardItem(tr("隐藏规则")), new QStandardItem(tr("暂无缓存")),
                new QStandardItem(tr("--")), new QStandardItem(tr("--")),
                new QStandardItem(tr("--")), new QStandardItem(tr("--"))};
            row.first()->setData(number, Qt::UserRole);
            row.first()->setData(true, Qt::UserRole + 1);
            m_model->appendRow(row);
            visibleNumbers.append(number);
        }
        for (int row = 0; row < m_model->rowCount(); ++row) {
            if (m_model->item(row, 0)->data(Qt::UserRole).toString().compare(previousNumber, Qt::CaseInsensitive) == 0) {
                m_table->selectRow(row);
                break;
            }
        }
        updateToggleButton();
    }

    void addTrain()
    {
        domain::Train train;
        const QVector<RailwayStation> stations = availableStations(m_dataStore);
        if (addTrainForm(this, stations, &train))
            reportResult(this, m_service.addTrain(train), {});
    }

    void toggleTrainVisibility()
    {
        const QString number = selectedNumber();
        if (number.isEmpty()) {
            QMessageBox::information(this, tr("车次显示状态"), tr("请先选择一个车次。"));
            return;
        }
        const bool hidden = selectedTrainIsHidden();
        const QString title = hidden ? tr("恢复车次") : tr("隐藏车次");
        const QString prompt = hidden
                                   ? tr("确定恢复显示车次 %1 吗？").arg(number)
                                   : tr("确定隐藏车次 %1 吗？以后 12306 再返回该车次时也不会显示。").arg(number);
        if (QMessageBox::question(this, title, prompt,
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
            == QMessageBox::Yes) {
            reportResult(this, hidden ? m_service.restoreTrain(number)
                                      : m_service.removeTrain(number, true), {});
        }
    }

    DataStore *m_dataStore;
    AdminService m_service;
    RailwayQueryService *m_railwayService;
    QTableView *m_table;
    QStandardItemModel *m_model;
    QPushButton *m_toggleButton;
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
        configureTable(m_table);
        m_table->setModel(m_model);
        m_table->setEditTriggers(QAbstractItemView::SelectedClicked
                                 | QAbstractItemView::DoubleClicked
                                 | QAbstractItemView::EditKeyPressed);
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
        for (const domain::Train &train : m_dataStore->data().trains) {
            if (!train.railwayServiceDate.isValid()
                && !m_dataStore->data().hiddenTrainNumbers.contains(train.number, Qt::CaseInsensitive))
                m_trainCombo->addItem(train.number, train.number);
        }
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
        for (const RailwayStation &station : availableStations(m_dataStore))
            stations.append(station.code + QStringLiteral(" - ") + station.name);
        bool ok = false;
        const QString selected = QInputDialog::getItem(this, tr("新增经停站"), tr("车站"), stations, 0, false, &ok);
        if (!ok || selected.isEmpty())
            return;
        const QString code = selected.section(QStringLiteral(" - "), 0, 0);
        m_model->appendRow({new QStandardItem(code),
                            new QStandardItem(QString()),
                            new QStandardItem(QString()),
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
                         tr("时刻与经停站已保存，原席别、票价和座位占用已尽量保留。"))) {
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
        layout->addWidget(new QLabel(tr("单击票价、总票额或余票单元格即可编辑；保存后按总票额生成具体席位。票价单位为元。"), this));
        configureTable(m_table);
        m_table->setModel(m_model);
        m_table->setEditTriggers(QAbstractItemView::SelectedClicked
                                 | QAbstractItemView::DoubleClicked
                                 | QAbstractItemView::EditKeyPressed);
        m_table->setItemDelegateForColumn(2, new MoneyDelegate(m_table));
        m_table->setItemDelegateForColumn(3, new CountDelegate(m_table));
        m_table->setItemDelegateForColumn(4, new CountDelegate(m_table));
        layout->addWidget(m_table);
        auto *actions = new QHBoxLayout;
        auto *addButton = new QPushButton(tr("新增席别"), this);
        auto *removeButton = new QPushButton(tr("删除席别"), this);
        auto *detailsButton = new QPushButton(tr("具体席位"), this);
        auto *saveButton = new QPushButton(tr("保存"), this);
        auto *closeButton = new QPushButton(tr("关闭"), this);
        actions->addWidget(addButton);
        actions->addWidget(removeButton);
        actions->addWidget(detailsButton);
        actions->addStretch();
        actions->addWidget(saveButton);
        actions->addWidget(closeButton);
        layout->addLayout(actions);
        connect(m_trainCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() { refreshRows(); });
        connect(addButton, &QPushButton::clicked, this, [this]() { addSeat(); });
        connect(removeButton, &QPushButton::clicked, this, [this]() { removeSeat(); });
        connect(detailsButton, &QPushButton::clicked, this, [this]() { showSeatDetails(); });
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
        for (const domain::Train &train : m_dataStore->data().trains) {
            if (!train.railwayServiceDate.isValid()
                && !m_dataStore->data().hiddenTrainNumbers.contains(train.number, Qt::CaseInsensitive))
                m_trainCombo->addItem(train.number, train.number);
        }
        refreshRows();
    }

    void appendSegmentRow(const QString &seatType,
                          const QString &segmentName,
                          const domain::SegmentInventory &segment)
    {
        auto *segmentItem = new QStandardItem(segmentName);
        segmentItem->setEditable(false);
        auto *seatItem = new QStandardItem(seatType);
        seatItem->setEditable(false);
        m_model->appendRow({seatItem,
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
                const QString localizedSegmentName =
                    stationDisplayName(m_dataStore->data(), train->stops.at(index).stationCode)
                    + QStringLiteral(" → ")
                    + stationDisplayName(m_dataStore->data(), train->stops.at(index + 1).stationCode);
                appendSegmentRow(seat.seatType, localizedSegmentName, seat.segments.at(index));
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
        for (int row = 0; row < m_model->rowCount(); ++row) {
            if (m_model->item(row, 0)->text().compare(seatType, Qt::CaseInsensitive) == 0) {
                QMessageBox::warning(this, tr("新增席别"), tr("席别 %1 已存在。").arg(seatType));
                return;
            }
        }
        for (int index = 0; index < train->stops.size() - 1; ++index) {
            const QString segmentName =
                stationDisplayName(m_dataStore->data(), train->stops.at(index).stationCode)
                + QStringLiteral(" → ")
                + stationDisplayName(m_dataStore->data(), train->stops.at(index + 1).stationCode);
            appendSegmentRow(seatType, segmentName, {0, 500, 500});
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

    void showSeatDetails()
    {
        const QModelIndex selected = m_table->currentIndex();
        if (!selected.isValid()) {
            QMessageBox::information(this, tr("具体席位"), tr("请先选择一个席别。"));
            return;
        }
        const QString seatType = m_model->item(selected.row(), 0)->text();
        const domain::Train *train = trainByNumber(m_dataStore->data(), currentTrainNumber());
        if (!train)
            return;
        const auto seat = std::find_if(train->seats.cbegin(), train->seats.cend(), [&seatType](const domain::SeatInventory &item) {
            return item.seatType.compare(seatType, Qt::CaseInsensitive) == 0;
        });
        if (seat == train->seats.cend()) {
            QMessageBox::information(this, tr("具体席位"), tr("该席别尚未保存，请先保存主表。"));
            return;
        }

        QDialog dialog(this);
        dialog.setWindowTitle(tr("%1 · %2 · 具体席位").arg(currentTrainNumber(), seat->seatType));
        dialog.resize(760, 580);
        auto *layout = new QVBoxLayout(&dialog);
        auto *table = new QTableView(&dialog);
        auto *model = new QStandardItemModel(0, 3, table);
        model->setHorizontalHeaderLabels({tr("席位号"), tr("区间占用（二进制，可编辑）"),
                                          tr("已占用区间")});
        const int maskWidth = seat->segments.size();
        QHash<QString, QString> stationNames;
        for (const domain::Station &station : m_dataStore->data().stations)
            stationNames.insert(station.code, station.name);
        const auto occupiedText = [train, stationNames](quint64 mask) {
            QStringList segments;
            for (int index = 0; index + 1 < train->stops.size(); ++index) {
                if ((mask & (quint64(1) << index)) != 0) {
                    const QString fromCode = train->stops.at(index).stationCode;
                    const QString toCode = train->stops.at(index + 1).stationCode;
                    segments.append(stationNames.value(fromCode, fromCode) + QStringLiteral("→")
                                    + stationNames.value(toCode, toCode));
                }
            }
            return segments.isEmpty() ? QObject::tr("空闲") : segments.join(QStringLiteral("、"));
        };
        for (const domain::SeatDetail &detail : seat->details) {
            auto *id = new QStandardItem(detail.seatId);
            id->setEditable(false);
            auto *binary = new QStandardItem(QString::number(detail.occupiedMask, 2)
                                                 .rightJustified(maskWidth, QLatin1Char('0')));
            auto *occupied = new QStandardItem(occupiedText(detail.occupiedMask));
            occupied->setEditable(false);
            model->appendRow({id, binary, occupied});
        }
        configureTable(table);
        table->setModel(model);
        table->setEditTriggers(QAbstractItemView::SelectedClicked
                               | QAbstractItemView::DoubleClicked
                               | QAbstractItemView::EditKeyPressed);
        connect(model, &QStandardItemModel::itemChanged, &dialog,
                [model, maskWidth, occupiedText, updating = false](QStandardItem *item) mutable {
            if (!item || updating || item->column() != 1)
                return;
            updating = true;
            bool ok = false;
            const QString binary = item->text().trimmed();
            ok = !binary.isEmpty() && binary.size() <= maskWidth;
            for (const QChar digit : binary)
                ok = ok && (digit == QLatin1Char('0') || digit == QLatin1Char('1'));
            const quint64 mask = ok ? binary.toULongLong(&ok, 2) : 0;
            if (ok)
                item->setText(binary.rightJustified(maskWidth, QLatin1Char('0')));
            model->item(item->row(), 2)->setText(ok ? occupiedText(mask) : QObject::tr("无效"));
            updating = false;
        });
        layout->addWidget(table);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Close, &dialog);
        buttons->button(QDialogButtonBox::Save)->setText(tr("保存席位状态"));
        buttons->button(QDialogButtonBox::Close)->setText(tr("关闭"));
        layout->addWidget(buttons);
        connect(buttons->button(QDialogButtonBox::Save), &QPushButton::clicked, &dialog, [&, model]() {
            QVector<domain::SeatDetail> details;
            details.reserve(model->rowCount());
            const quint64 validMask = domain::segmentMask(0, seat->segments.size());
            for (int row = 0; row < model->rowCount(); ++row) {
                bool ok = false;
                const QString binary = model->item(row, 1)->text().trimmed();
                bool validBinary = !binary.isEmpty() && binary.size() <= maskWidth;
                for (const QChar digit : binary)
                    validBinary = validBinary
                        && (digit == QLatin1Char('0') || digit == QLatin1Char('1'));
                const quint64 mask = validBinary ? binary.toULongLong(&ok, 2) : 0;
                if (!ok || (mask & ~validMask) != 0) {
                    QMessageBox::warning(&dialog, tr("掩码错误"),
                                         tr("第 %1 行不是有效的二进制区间掩码。").arg(row + 1));
                    return;
                }
                details.append({model->item(row, 0)->text(), mask});
            }
            if (reportResult(&dialog,
                             m_service.replaceSeatDetails(currentTrainNumber(), seatType, details),
                             tr("具体席位占用状态已保存。"))) {
                dialog.accept();
                refreshRows();
            }
        });
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        applyStandardDialogStyle(&dialog);
        dialog.exec();
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
} // 命名空间

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

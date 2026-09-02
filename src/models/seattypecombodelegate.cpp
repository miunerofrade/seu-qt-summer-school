#include "models/seattypecombodelegate.h"

#include "models/trainquerymodel.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QTimer>

#include <algorithm>

SeatTypeComboDelegate::SeatTypeComboDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void SeatTypeComboDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const
{
    QStyleOptionViewItem baseOption(option);
    initStyleOption(&baseOption, index);
    baseOption.text.clear();
    QStyle *style = baseOption.widget ? baseOption.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &baseOption, painter, baseOption.widget);

    const bool selected = option.state.testFlag(QStyle::State_Selected);
    const QColor color = selected ? Qt::white : QColor(QStringLiteral("#111827"));
    const QRect contentRect = option.rect.adjusted(8, 0, -24, 0);

    painter->save();
    painter->setPen(color);
    painter->drawText(contentRect, Qt::AlignCenter, index.data(Qt::DisplayRole).toString());

    const int arrowX = option.rect.right() - 14;
    const int arrowY = option.rect.center().y();
    QPainterPath arrow;
    arrow.moveTo(arrowX - 3, arrowY - 2);
    arrow.lineTo(arrowX, arrowY + 1);
    arrow.lineTo(arrowX + 3, arrowY - 2);
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(QPen(color, 1.2));
    painter->drawPath(arrow);
    painter->restore();
}

QWidget *SeatTypeComboDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &,
                                              const QModelIndex &index) const
{
    auto *combo = new QComboBox(parent);
    combo->addItems(index.data(TrainQueryModel::SeatOptionsRole).toStringList());
    combo->setStyleSheet(QStringLiteral(R"(
QComboBox {
    color: #111827;
    background-color: #FFFFFF;
    border: 1px solid #1683FF;
    border-radius: 3px;
    padding: 2px 20px 2px 8px;
}
QComboBox::drop-down {
    border: none;
    width: 18px;
}
QComboBox::down-arrow {
    image: url(:/icons/chevron-down.svg);
    width: 10px;
    height: 10px;
}
QComboBox QAbstractItemView {
    color: #111827;
    background-color: #FFFFFF;
    selection-color: #FFFFFF;
    selection-background-color: #1683FF;
    border: 1px solid #D7DEE8;
    outline: 0;
}
)"));
    auto *delegate = const_cast<SeatTypeComboDelegate *>(this);
    connect(combo, &QComboBox::activated, delegate, [delegate, combo]() {
        emit delegate->commitData(combo);
        emit delegate->closeEditor(combo);
    });
    QTimer::singleShot(0, combo, [combo]() { combo->showPopup(); });
    return combo;
}

void SeatTypeComboDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    auto *combo = qobject_cast<QComboBox *>(editor);
    if (!combo)
        return;
    const int selected = combo->findText(index.data(Qt::DisplayRole).toString());
    combo->setCurrentIndex(std::max(0, selected));
}

void SeatTypeComboDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                         const QModelIndex &index) const
{
    const auto *combo = qobject_cast<QComboBox *>(editor);
    if (combo)
        model->setData(index, combo->currentText(), Qt::EditRole);
}

void SeatTypeComboDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                                                 const QModelIndex &) const
{
    editor->setGeometry(option.rect.adjusted(3, 2, -3, -2));
}

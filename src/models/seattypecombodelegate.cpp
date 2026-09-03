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

    // Only the display state is custom-painted. The actual editor created
    // below remains a normal QComboBox. Match the application's other combo
    // boxes here instead of asking the table's style to imitate one.
    const QRect controlRect = option.rect.adjusted(2, 0, -2, 0);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const bool selected = option.state.testFlag(QStyle::State_Selected);
    const QColor color = selected ? option.palette.color(QPalette::HighlightedText)
                                  : QColor(QStringLiteral("#2F3033"));
    painter->setPen(color);
    painter->drawText(controlRect.adjusted(4, 0, -4, 0),
                      Qt::AlignCenter,
                      index.data(Qt::DisplayRole).toString());

    const int arrowX = controlRect.right() - 11;
    const int arrowY = controlRect.center().y();
    QPainterPath arrow;
    arrow.moveTo(arrowX - 3, arrowY - 2);
    arrow.lineTo(arrowX, arrowY + 1);
    arrow.lineTo(arrowX + 3, arrowY - 2);
    painter->setPen(QPen(color, 1.2));
    painter->drawPath(arrow);
    painter->restore();
}

QWidget *SeatTypeComboDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &,
                                              const QModelIndex &index) const
{
    auto *combo = new QComboBox(parent);
    combo->setFrame(false);
    combo->addItems(index.data(TrainQueryModel::SeatOptionsRole).toStringList());
    combo->setAutoFillBackground(true);
    QPalette editorPalette = combo->palette();
    editorPalette.setColor(QPalette::Window, editorPalette.color(QPalette::Base));
    combo->setPalette(editorPalette);
    for (int item = 0; item < combo->count(); ++item) {
        combo->setItemData(item,
                           static_cast<int>(Qt::AlignCenter),
                           Qt::TextAlignmentRole);
    }
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
    editor->setGeometry(option.rect.adjusted(1, 1, -1, -1));
}

#ifndef SEATTYPECOMBODELEGATE_H
#define SEATTYPECOMBODELEGATE_H

#include <QStyledItemDelegate>

class SeatTypeComboDelegate final : public QStyledItemDelegate
{
public:
    explicit SeatTypeComboDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override;
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const override;
};

#endif // SEATTYPECOMBODELEGATE_H

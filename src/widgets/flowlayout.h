// Adapted from Qt's Flow Layout Example.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#ifndef FLOWLAYOUT_H
#define FLOWLAYOUT_H

#include <QLayout>
#include <QList>
#include <QStyle>

class FlowLayout : public QLayout
{
public:
    explicit FlowLayout(QWidget *parent = nullptr,
                        int margin = -1,
                        int horizontalSpacing = -1,
                        int verticalSpacing = -1);
    explicit FlowLayout(int margin = -1,
                        int horizontalSpacing = -1,
                        int verticalSpacing = -1);
    ~FlowLayout() override;

    void addItem(QLayoutItem *item) override;
    int horizontalSpacing() const;
    int verticalSpacing() const;
    Qt::Orientations expandingDirections() const override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int width) const override;
    int count() const override;
    QLayoutItem *itemAt(int index) const override;
    QSize minimumSize() const override;
    void setGeometry(const QRect &rect) override;
    QSize sizeHint() const override;
    QLayoutItem *takeAt(int index) override;

private:
    int doLayout(const QRect &rect, bool testOnly) const;
    int smartSpacing(QStyle::PixelMetric pixelMetric) const;

    QList<QLayoutItem *> m_items;
    int m_horizontalSpacing;
    int m_verticalSpacing;
};

#endif // FLOWLAYOUT_H

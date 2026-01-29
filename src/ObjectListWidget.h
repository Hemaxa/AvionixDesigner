#ifndef OBJECTLISTWIDGET_H
#define OBJECTLISTWIDGET_H

#include <QListWidget>
#include <QList>
#include <QSharedPointer>
#include "shapes.h"

/**
 * @brief Виджет списка объектов
 */
class ObjectListWidget : public QListWidget
{
    Q_OBJECT

public:
    explicit ObjectListWidget(QWidget *parent = nullptr);

    // Обновить список объектов
    void setShapes(const QList<QSharedPointer<CorelShape>> &shapes);

signals:
    // Сигнал при выборе объекта (индекс в списке)
    void objectSelected(int index);

private slots:
    void onItemClicked(QListWidgetItem *item);
};

#endif // OBJECTLISTWIDGET_H

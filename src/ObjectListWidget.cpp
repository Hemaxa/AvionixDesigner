#include "ObjectListWidget.h"

ObjectListWidget::ObjectListWidget(QWidget *parent)
    : QListWidget(parent)
{
    setMinimumWidth(150);
    connect(this, &QListWidget::itemClicked, this, &ObjectListWidget::onItemClicked);
}

void ObjectListWidget::setShapes(const QList<QSharedPointer<CorelShape>> &shapes)
{
    clear();
    
    int index = 0;
    for (const auto &shape : shapes) {
        QString typeName;
        
        // Определяем тип объекта
        if (dynamic_cast<CorelRect*>(shape.data())) {
            typeName = "Rectangle";
        } else if (dynamic_cast<CorelRotationObject*>(shape.data())) {
            typeName = "RotationObject";
        } else {
            typeName = "Unknown";
        }
        
        QListWidgetItem *item = new QListWidgetItem(QString("%1. %2").arg(index + 1).arg(typeName));
        item->setData(Qt::UserRole, index);
        addItem(item);
        index++;
    }
}

void ObjectListWidget::onItemClicked(QListWidgetItem *item)
{
    int index = item->data(Qt::UserRole).toInt();
    emit objectSelected(index);
}

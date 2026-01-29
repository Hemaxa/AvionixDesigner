/**
 * @file ObjectsListWindow.cpp
 * @brief Реализация окна списка объектов
 */

#include "ObjectsListWindow.h"
#include "../managers/ProjectManager.h"

ObjectsListWindow::ObjectsListWindow(QWidget *parent)
    : QListWidget(parent)
{
    setMinimumWidth(150);
    
    connect(ProjectManager::instance(), &ProjectManager::projectLoaded,
            this, &ObjectsListWindow::refreshList);
    
    connect(this, &QListWidget::currentRowChanged,
            this, &ObjectsListWindow::onRowChanged);
}

void ObjectsListWindow::refreshList()
{
    clear();
    
    auto pm = ProjectManager::instance();
    const auto &objects = pm->getObjects();
    
    for (int i = 0; i < objects.size(); ++i) {
        const auto &obj = objects[i];
        
        QString text = QString("%1. %2")
            .arg(i + 1)
            .arg(obj->typeName());
        
        QListWidgetItem *item = new QListWidgetItem(text);
        item->setData(Qt::UserRole, i);
        addItem(item);
    }
}

void ObjectsListWindow::onRowChanged(int row)
{
    if (row >= 0) {
        emit objectSelected(row);
    }
}

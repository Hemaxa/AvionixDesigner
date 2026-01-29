/**
 * @file ObjectsListWindow.h
 * @brief Окно со списком объектов проекта
 */

#pragma once

#include <QListWidget>

/**
 * @class ObjectsListWindow
 * @brief Виджет списка объектов
 */
class ObjectsListWindow : public QListWidget
{
    Q_OBJECT
    
public:
    explicit ObjectsListWindow(QWidget *parent = nullptr);
    
    void refreshList();

signals:
    void objectSelected(int index);

private slots:
    void onRowChanged(int row);
};

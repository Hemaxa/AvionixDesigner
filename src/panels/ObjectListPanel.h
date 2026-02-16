//ObjectListPanel - панель со списком объектов проекта

#pragma once

#include "BasePanel.h"

class QListWidget;
class QVBoxLayout;

class ObjectListPanel : public BasePanel
{
    Q_OBJECT
    
public:
    explicit ObjectListPanel(QWidget *parent = nullptr);
    
    //обновляет список объектов из проекта
    void refreshList();

signals:
    //сигнал выбора объекта по индексу
    void objectSelected(int index);

private slots:
    //обработчик смены выбранной строки
    void onRowChanged(int row);

private:
    QListWidget *m_listWidget; //виджет списка
};

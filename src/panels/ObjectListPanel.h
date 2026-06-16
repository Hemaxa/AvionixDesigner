//ObjectListPanel - панель со списком объектов проекта

#pragma once

#include "BasePanel.h"
#include <QModelIndex>

class QListWidget;
class QVBoxLayout;

class ObjectListPanel : public BasePanel
{
    Q_OBJECT
    
public:
    explicit ObjectListPanel(QWidget *parent = nullptr);
    
    //обновляет список объектов из проекта
    void refreshList();
    
    //выбирает строку программно
    void selectRow(int index);

signals:
    //сигнал выбора объекта по индексу
    void objectSelected(int index);

private slots:
    //обработчик смены выбранной строки
    void onRowChanged(int row);
    void onRowsMoved(const QModelIndex &parent, int start, int end, const QModelIndex &destination, int row);

private:
    QListWidget *m_listWidget; //виджет списка
    bool m_refreshing = false;
};

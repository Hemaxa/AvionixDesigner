//ObjectListPanel - панель со списком объектов проекта

#pragma once

#include "BasePanel.h"
#include <QList>
#include <QModelIndex>

class QListWidget;
class QListWidgetItem;
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
    void selectRows(const QList<int> &indexes);

signals:
    //сигнал выбора объекта по индексу
    void objectSelected(int index);
    void selectionChanged(const QList<int> &indexes);

private slots:
    //обработчик смены выбранной строки
    void onRowChanged(int row);
    void onSelectionChanged();
    void onRowsMoved(const QModelIndex &parent, int start, int end, const QModelIndex &destination, int row);
    void onItemDoubleClicked(QListWidgetItem *item);

private:
    QList<int> selectedObjectIndexes() const;
    QListWidget *m_listWidget; //виджет списка
    bool m_refreshing = false;
};

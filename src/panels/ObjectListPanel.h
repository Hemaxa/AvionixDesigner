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
    
    //выбирает объекты по индексам проекта, независимо от строк групп в списке
    void selectRows(const QList<int> &indexes);

signals:
    //сигнал выбора одного объекта; -1 означает пустое или множественное выделение
    void objectSelected(int index);
    //сигнал выбора набора объектов по индексам проекта
    void selectionChanged(const QList<int> &indexes);

private slots:
    //обрабатывает смену текущей строки списка
    void onRowChanged(int row);
    //синхронизирует внешнее выделение с выбранными строками списка
    void onSelectionChanged();
    //перестраивает порядок объектов после перетаскивания строк списка
    void onRowsMoved(const QModelIndex &parent, int start, int end, const QModelIndex &destination, int row);
    //переименовывает объект или группу по двойному клику
    void onItemDoubleClicked(QListWidgetItem *item);

private:
    //возвращает индексы объектов, выбранных через строки объектов или групп
    QList<int> selectedObjectIndexes() const;
    QListWidget *m_listWidget; //виджет списка
    bool m_refreshing = false;
};

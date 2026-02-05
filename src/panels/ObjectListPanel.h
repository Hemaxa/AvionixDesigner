/**
 * @file ObjectListPanel.h
 * @brief Панель со списком объектов проекта
 */

#pragma once

#include "../BasePanel.h"

class QListWidget;
class QVBoxLayout;

/**
 * @class ObjectListPanel
 * @brief Виджет списка объектов сцены
 */
class ObjectListPanel : public BasePanel
{
    Q_OBJECT
    
public:
    explicit ObjectListPanel(QWidget *parent = nullptr);
    
    // Обновляет список объектов из проекта
    void refreshList();

signals:
    // Сигнал выбора объекта по индексу
    void objectSelected(int index);

private slots:
    // Обработчик смены выбранной строки
    void onRowChanged(int row);

private:
    QListWidget *m_listWidget;  // Виджет списка
};

/**
 * @file ObjectPropertiesPanel.h
 * @brief Панель свойств выбранного объекта
 */

#pragma once

#include "BasePanel.h"
#include <QSharedPointer>

class QTableWidget;
class QLabel;
class QScrollArea;
class BaseObject;

/**
 * @class ObjectPropertiesPanel
 * @brief Виджет отображения и редактирования свойств выбранного объекта
 */
class ObjectPropertiesPanel : public BasePanel
{
    Q_OBJECT
    
public:
    explicit ObjectPropertiesPanel(QWidget *parent = nullptr);
    
public slots:
    // Показывает свойства объекта по индексу
    void showObjectProperties(int index);
    
    // Показывает свойства переданного объекта
    void showProperties(QSharedPointer<BaseObject> obj);
    
    // Очищает панель свойств
    void clearProperties();

signals:
    // Сигнал об изменении свойства объекта
    void propertyChanged();

private slots:
    // Обработка изменения ячейки таблицы
    void onCellChanged(int row, int column);

private:
    // Заполняет таблицу свойствами объекта
    void populateTable(const QList<QPair<QString, QString>> &props);
    
    // Очищает таблицу
    void clearTable();
    
    QLabel *m_titleLabel;                       // Заголовок панели
    QTableWidget *m_tableWidget;                // Таблица свойств
    QScrollArea *m_scrollArea;                  // Область прокрутки
    QSharedPointer<BaseObject> m_currentObject; // Текущий объект
    bool m_updating = false;                    // Флаг программного обновления
};

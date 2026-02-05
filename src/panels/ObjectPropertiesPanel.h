/**
 * @file ObjectPropertiesPanel.h
 * @brief Панель свойств выбранного объекта
 */

#pragma once

#include "../BasePanel.h"
#include <QSharedPointer>

class QFormLayout;
class QLabel;
class QScrollArea;
class BaseObject;

/**
 * @class ObjectPropertiesPanel
 * @brief Виджет отображения свойств выбранного объекта
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

private:
    // Добавляет свойство в форму
    void addProperty(const QString &name, const QString &value);
    
    // Очищает форму
    void clearForm();
    
    QLabel *m_titleLabel;       // Заголовок панели
    QFormLayout *m_formLayout;  // Layout для свойств
    QScrollArea *m_scrollArea;  // Область прокрутки
};

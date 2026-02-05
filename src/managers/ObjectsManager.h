/**
 * @file ObjectsManager.h
 * @brief Менеджер создания объектов (паттерн фабрика)
 */

#pragma once

#include <QObject>
#include <QMap>
#include <QSharedPointer>
#include <QDomElement>
#include <functional>

#include "../BaseObject.h"

/**
 * @class ObjectsManager
 * @brief Фабрика для создания объектов по типу (синглтон)
 */
class ObjectsManager : public QObject
{
    Q_OBJECT
    
public:
    // Тип функции-конструктора объекта
    using ObjectCreator = std::function<BaseObject*()>;
    
    // Получение единственного экземпляра
    static ObjectsManager* instance();
    
    // Регистрирует тип объекта
    void registerType(const QString &typeName, ObjectCreator creator);
    
    // Создаёт объект по имени типа
    BaseObject* createObject(const QString &typeName);
    
    // Проверяет, зарегистрирован ли тип
    bool hasType(const QString &typeName) const;
    
    // Парсит схемы параметров из секции <parameters>
    QMap<QString, ParamSchema> parseSchemas(const QDomElement &parametersNode);
    
    // Возвращает список зарегистрированных типов
    QStringList registeredTypes() const;

private:
    ObjectsManager() = default;
    QMap<QString, ObjectCreator> m_creators;  // Карта типов и их конструкторов
};

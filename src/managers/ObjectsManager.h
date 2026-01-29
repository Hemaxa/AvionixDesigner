//ObjectsManager - менеджер создания объектов (приницп фабрики)

#pragma once

#include <QObject>
#include <QMap>
#include <QSharedPointer>
#include <QDomElement> //работа с XML
#include <functional>

#include "../objects/AbstractObject.h"

class ObjectsManager : public QObject
{
    Q_OBJECT
    
public:
    //Тип функции-конструктора объекта
    using ObjectCreator = std::function<AbstractObject*()>;
    
    static ObjectsManager* instance();
    
    //регистрирует тип объекта, вызывается в registerStandardTypes
    //typeName - имя типа, creator - функция для создания объекта
    void registerType(const QString &typeName, ObjectCreator creator);
    
    //typeName Имя типа объекта
    AbstractObject* createObject(const QString &typeName);
    
    /**
     * @brief Проверяет, зарегистрирован ли тип
     */
    bool hasType(const QString &typeName) const;
    
    /**
     * @brief Парсит схемы параметров из секции <parameters>
     */
    QMap<QString, ParamSchema> parseSchemas(const QDomElement &parametersNode);
    
    /**
     * @brief Возвращает список зарегистрированных типов
     */
    QStringList registeredTypes() const;

private:
    ObjectsManager() = default;
    QMap<QString, ObjectCreator> m_creators;
};

//ObjectsManager - менеджер создания объектов по принципу фабрики

#pragma once

#include <QObject>
#include <QMap>
#include <QSharedPointer>
#include <QDomElement>
#include <functional>

#include "BaseObject.h"

class ObjectsManager : public QObject
{
    Q_OBJECT
    
public:
    //получение единственного экземпляра
    static ObjectsManager* instance();

    //тип функции-конструктора объекта
    using ObjectCreator = std::function<BaseObject*()>;
    
    //метод регистрации конкретного типа объекта
    void registerType(const QString &typeName, ObjectCreator creator);
    
    //создает объект по имени типа
    BaseObject* createObject(const QString &typeName);
    
    //проверяет, зарегистрирован ли тип
    bool hasType(const QString &typeName) const;
    
    //парсит схемы параметров из секции <parameters>
    QMap<QString, ParamSchema> parseSchemas(const QDomElement &parametersNode);
    
    //возвращает список зарегистрированных типов
    QStringList registeredTypes() const;

private:
    ObjectsManager() = default;
    QMap<QString, ObjectCreator> m_creators;  //карта типов и их конструкторов
};

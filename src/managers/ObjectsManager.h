// ObjectsManager - менеджер создания объектов по принципу фабрики.

#pragma once

#include <QObject>
#include <QMap>
#include <functional>

#include "BaseObject.h"

// ObjectsManager - менеджер создания объектов по принципу фабрики.
class ObjectsManager : public QObject
{
    Q_OBJECT

public:
    // получение единственного экземпляра
    static ObjectsManager* instance();

    // тип функции-конструктора объекта
    using ObjectCreator = std::function<BaseObject*()>;

    // метод регистрации конкретного типа объекта
    void registerType(const QString &typeName, ObjectCreator creator);

    // создает объект по имени типа
    BaseObject* createObject(const QString &typeName);

private:
    ObjectsManager() = default;
    QMap<QString, ObjectCreator> m_creators;  //карта типов и их конструкторов
};

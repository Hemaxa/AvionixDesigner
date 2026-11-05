// ObjectsManager - менеджер создания объектов по принципу фабрики.

#include "ObjectsManager.h"

ObjectsManager* ObjectsManager::instance()
{
    static ObjectsManager s_instance;
    return &s_instance;
}

void ObjectsManager::registerType(const QString &typeName, ObjectCreator creator)
{
    m_creators[typeName] = creator;
}

BaseObject* ObjectsManager::createObject(const QString &typeName)
{
    // создаём объект через зарегистрированный конструктор
    if (m_creators.contains(typeName)) {
        return m_creators[typeName]();
    }
    return nullptr;
}

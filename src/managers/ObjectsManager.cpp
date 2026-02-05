/**
 * @file ObjectsManager.cpp
 * @brief Реализация менеджера создания объектов
 */

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
    // Создаём объект через зарегистрированный конструктор
    if (m_creators.contains(typeName)) {
        return m_creators[typeName]();
    }
    return nullptr;
}

bool ObjectsManager::hasType(const QString &typeName) const
{
    return m_creators.contains(typeName);
}

QMap<QString, ParamSchema> ObjectsManager::parseSchemas(const QDomElement &parametersNode)
{
    QMap<QString, ParamSchema> schemas;
    
    if (parametersNode.isNull()) return schemas;
    
    // Проходим по всем типам объектов
    QDomNode typeNode = parametersNode.firstChild();
    while (!typeNode.isNull()) {
        QDomElement typeEl = typeNode.toElement();
        if (!typeEl.isNull()) {
            QString typeName = typeEl.tagName();
            ParamSchema schema;
            
            // Читаем все параметры типа
            QDomNode paramNode = typeEl.firstChild();
            while (!paramNode.isNull()) {
                QDomElement paramEl = paramNode.toElement();
                if (!paramEl.isNull()) {
                    ParamInfo info;
                    info.offset = paramEl.attribute("offset").toInt();
                    info.size = paramEl.attribute("size").toInt();
                    schema.insert(paramEl.tagName(), info);
                }
                paramNode = paramNode.nextSibling();
            }
            
            schemas.insert(typeName, schema);
        }
        typeNode = typeNode.nextSibling();
    }
    
    return schemas;
}

QStringList ObjectsManager::registeredTypes() const
{
    return m_creators.keys();
}

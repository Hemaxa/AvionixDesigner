#include <QFile>
#include <QDomDocument>
#include <QTextStream>

#include "ProjectManager.h"
#include "ObjectsManager.h"
#include "RectangleObject.h"
#include "RotationObject.h"
#include "StaticGroupObject.h"
#include "BitParser.h"
#include "DebugDumper.h"

ProjectManager::ProjectManager() : m_canvasWidth(0), m_canvasHeight(0), m_bgColor() {}

ProjectManager* ProjectManager::instance()
{
    static ProjectManager s_instance;
    return &s_instance;
}

bool ProjectManager::loadFromFile(const QString &fileName)
{
    m_filePath = fileName;
    m_objects.clear();
    
    emit logMessage(tr("Загрузка: %1").arg(fileName));
    
    //открываем файл
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        emit logMessage(tr("ОШИБКА: Не удалось открыть файл"));
        return false;
    }
    
    //парсим XML
    QDomDocument doc;
    QDomDocument::ParseResult result = doc.setContent(&file);
    if (!result) {
        emit logMessage(tr("ОШИБКА XML: %1").arg(result.errorMessage));
        file.close();
        return false;
    }
    file.close();
    
    //получаем корневой элемент
    QDomElement root = doc.documentElement();
    
    //читаем метаданные проекта
    m_projectName = root.attribute("name", "Untitled");
    m_canvasWidth = root.attribute("width", "640").toInt();
    m_canvasHeight = root.attribute("height", "480").toInt();
    
    //парсим цвет фона
    QString bgStr = root.attribute("bgcolor", "#0");
    if (bgStr.startsWith("#")) {
        bool ok;
        uint colorVal = bgStr.mid(1).toUInt(&ok, 16);
        if (ok) {
            m_bgColor = QColor(colorVal & 0xFF, (colorVal >> 8) & 0xFF, (colorVal >> 16) & 0xFF);
        }
    }
    
    emit logMessage(tr("Проект: %1 (%2x%3)").arg(m_projectName).arg(m_canvasWidth).arg(m_canvasHeight));
    
    //парсим схемы параметров и сохраняем для использования при сохранении
    QDomElement paramsEl = root.firstChildElement("parameters");
    m_schemas = ObjectsManager::instance()->parseSchemas(paramsEl);
    
    //маппинг тегов на схемы параметров (для тегов без собственной схемы)
    m_schemaAliases.clear();
    m_schemaAliases["rectangle_a"] = "rectangle";
    //m_schemaAliases["rectangle_e"] = "rectangle";
    
    for (const QString &type : m_schemas.keys()) {
        emit logMessage(tr("Схема: %1 (%2 полей)").arg(type).arg(m_schemas[type].size()));
    }
    
    //парсим объекты
    QDomElement objectsEl = root.firstChildElement("objects");
    QDomNode objNode = objectsEl.isNull() ? root.firstChild() : objectsEl.firstChild();
    
    //списки для отладочного дампа
    QList<QDomElement> debugElements;
    QStringList debugTypes;

    while (!objNode.isNull()) {
        QDomElement objEl = objNode.toElement();
        QString tagName = objEl.tagName();
        
        //определяем имя схемы (может отличаться от tagName)
        QString schemaName = m_schemaAliases.value(tagName, tagName);
        
        if (m_schemas.contains(schemaName)) {
            QString hexInit = objEl.firstChildElement("init").text().trimmed();
            
            //создаем объект через фабрику
            BaseObject *obj = ObjectsManager::instance()->createObject(tagName);
            
            if (obj && !hexInit.isEmpty()) {
                obj->parse(hexInit, m_schemas[schemaName]);
                obj->parseExtraData(objEl);
                
                m_objects.append(QSharedPointer<BaseObject>(obj));
                
                //сохраняем данные для дампа
                debugElements.append(objEl);
                debugTypes.append(tagName);
            }
        }
        
        objNode = objNode.nextSibling();
    }
    
    emit logMessage(tr("Загружено объектов: %1").arg(m_objects.size()));
    
    //формируем отладочный дамп парсинга
    DebugDumper::dumpToFile(fileName, m_objects, m_schemas, debugElements, debugTypes);
    emit logMessage(tr("Отладочный дамп сформирован"));
    
    emit projectLoaded();
    
    return true;
}

//метод регистрации стандартых типов объектов (получение словаря поддерживаемых объектов)
void ProjectManager::registerStandardTypes()
{
    //создание экземпляра менеджера объектов
    auto om = ObjectsManager::instance();
    
    //регистрируем типы объектов
    om->registerType("rectangle", []() { return new RectangleObject(); });
    om->registerType("rectangle_a", []() { return new RectangleObject(); });
    om->registerType("rectangle_e", []() { return new RectangleObject(); });
    om->registerType("rotationobject", []() { return new RotationObject(); });
    om->registerType("staticgroup", []() { return new StaticGroupObject(); });
}

bool ProjectManager::saveToFile()
{
    if (m_filePath.isEmpty()) return false;
    
    //переоткрываем оригинальный XML
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;
    
    QDomDocument doc;
    if (!doc.setContent(&file)) {
        file.close();
        return false;
    }
    file.close();
    
    QDomElement root = doc.documentElement();
    
    //обновляем bgcolor
    uint bgVal = (m_bgColor.blue() << 16) | (m_bgColor.green() << 8) | m_bgColor.red();
    root.setAttribute("bgcolor", QString("#%1").arg(bgVal, 0, 16));
    
    //обходим XML-объекты и обновляем <init> HEX-строки
    QDomElement objectsEl = root.firstChildElement("objects");
    QDomNode objNode = objectsEl.isNull() ? root.firstChild() : objectsEl.firstChild();
    
    int objIdx = 0;
    while (!objNode.isNull() && objIdx < m_objects.size()) {
        QDomElement objEl = objNode.toElement();
        if (!objEl.isNull()) {
            QString tagName = objEl.tagName();
            QString schemaName = m_schemaAliases.value(tagName, tagName);
            
            if (m_schemas.contains(schemaName)) {
                const ParamSchema &schema = m_schemas[schemaName];
                auto obj = m_objects[objIdx];
                
                //получаем карту параметров от объекта
                QMap<QString, quint32> params = obj->serializeParams();
                
                //обновляем первый <init>
                QDomElement initEl = objEl.firstChildElement("init");
                if (!initEl.isNull() && !params.isEmpty()) {
                    QString hexStr = initEl.text().trimmed();
                    
                    //инжектируем каждый параметр обратно в HEX-строку
                    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
                        if (schema.contains(it.key())) {
                            const ParamInfo &info = schema[it.key()];
                            BitParser::inject(hexStr, info.offset, info.size, it.value());
                        }
                    }
                    
                    //обновляем текстовое содержимое <init>
                    QDomNode textNode = initEl.firstChild();
                    if (!textNode.isNull() && textNode.isText()) {
                        textNode.setNodeValue(hexStr);
                    }
                }
                
                //для StaticGroupObject обновляем дополнительные <init> элементы
                auto staticGroup = dynamic_cast<StaticGroupObject*>(obj.data());
                if (staticGroup && staticGroup->states.size() > 1) {
                    QDomElement extraInit = initEl.isNull() ? QDomElement() : initEl.nextSiblingElement("init");
                    int stateIdx = 1;
                    
                    while (!extraInit.isNull() && stateIdx < staticGroup->states.size()) {
                        QMap<QString, quint32> stateParams = staticGroup->serializeState(stateIdx);
                        QString hexStr = extraInit.text().trimmed();
                        
                        for (auto it = stateParams.constBegin(); it != stateParams.constEnd(); ++it) {
                            if (schema.contains(it.key())) {
                                const ParamInfo &info = schema[it.key()];
                                BitParser::inject(hexStr, info.offset, info.size, it.value());
                            }
                        }
                        
                        QDomNode textNode = extraInit.firstChild();
                        if (!textNode.isNull() && textNode.isText()) {
                            textNode.setNodeValue(hexStr);
                        }
                        
                        extraInit = extraInit.nextSiblingElement("init");
                        stateIdx++;
                    }
                }
                
                objIdx++;
            }
        }
        objNode = objNode.nextSibling();
    }
    
    //записываем XML обратно в файл
    QFile outFile(m_filePath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    
    QTextStream stream(&outFile);
    doc.save(stream, 4);
    outFile.close();
    
    emit logMessage(tr("Проект сохранён: %1").arg(m_filePath));
    return true;
}

QSharedPointer<BaseObject> ProjectManager::getObjectAt(int index) const
{
    if (index >= 0 && index < m_objects.size()) {
        return m_objects[index];
    }
    return nullptr;
}

//геттеры
int ProjectManager::getObjectCount() const { return m_objects.size(); }
QString ProjectManager::getProjectName() const { return m_projectName; }
int ProjectManager::getCanvasWidth() const { return m_canvasWidth; }
int ProjectManager::getCanvasHeight() const { return m_canvasHeight; }
QColor ProjectManager::getBackgroundColor() const { return m_bgColor; }
QString ProjectManager::getFilePath() const { return m_filePath; }
const QList<QSharedPointer<BaseObject>>& ProjectManager::getObjects() const { return m_objects; }

//сеттеры
void ProjectManager::setBackgroundColor(const QColor &color)
{
    if (m_bgColor != color) {
        m_bgColor = color;
        emit projectChanged();
    }
}

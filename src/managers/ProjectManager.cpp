#include <QFile>
#include <QDomDocument>

#include "ProjectManager.h"
#include "ObjectsManager.h"
#include "../objects/RectangleObject.h"
#include "../objects/RotationObject.h"
#include "../objects/StaticGroupObject.h"

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
    
    //отправка сообщения в окно лога с неудачным открытием файла
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        emit logMessage(tr("ОШИБКА: Не удалось открыть файл"));
        return false;
    }
    
    //отправка сообщения в окно лога с неудачным парсингом файла
    QDomDocument doc;
    QDomDocument::ParseResult result = doc.setContent(&file);
    if (!result) {
        emit logMessage(tr("ОШИБКА XML: %1").arg(result.errorMessage));
        file.close();
        return false;
    }
    file.close();
    
    //создание XML структуры внутри программы
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
            //извелечение RGB со сдвигом
            m_bgColor = QColor(colorVal & 0xFF, (colorVal >> 8) & 0xFF, (colorVal >> 16) & 0xFF);
        }
    }
    
    emit logMessage(tr("Проект: %1 (%2x%3)").arg(m_projectName).arg(m_canvasWidth).arg(m_canvasHeight));
    
    //парсим схемы параметров
    QDomElement paramsEl = root.firstChildElement("parameters");
    QMap<QString, ParamSchema> schemas = ObjectsManager::instance()->parseSchemas(paramsEl);
    
    for (const QString &type : schemas.keys()) {
        emit logMessage(tr("Схема: %1 (%2 полей)").arg(type).arg(schemas[type].size()));
    }
    
    //парсим объекты
    QDomElement objectsEl = root.firstChildElement("objects");
    QDomNode objNode = objectsEl.isNull() ? root.firstChild() : objectsEl.firstChild();
    
    while (!objNode.isNull()) {
        QDomElement objEl = objNode.toElement();
        QString tagName = objEl.tagName();
        
        if (schemas.contains(tagName)) {
            QString hexInit = objEl.firstChildElement("init").text().trimmed();
            
            AbstractObject *obj = ObjectsManager::instance()->createObject(tagName);
            
            if (obj && !hexInit.isEmpty()) {
                obj->parse(hexInit, schemas[tagName]);
                obj->parseExtraData(objEl);
                m_objects.append(QSharedPointer<AbstractObject>(obj));
            }
        }
        
        objNode = objNode.nextSibling();
    }
    
    emit logMessage(tr("Загружено объектов: %1").arg(m_objects.size()));
    emit projectLoaded();
    
    return true;
}

void ProjectManager::registerStandardTypes()
{
    auto om = ObjectsManager::instance();
    
    om->registerType("rectangle", []() { return new RectangleObject(); });
    om->registerType("rectanglea", []() { return new RectangleObject(); });
    om->registerType("rectanglee", []() { return new RectangleObject(); });
    om->registerType("rotationobject", []() { return new RotationObject(); });
    om->registerType("staticgroup", []() { return new StaticGroupObject(); });
}

QSharedPointer<AbstractObject> ProjectManager::getObjectAt(int index) const
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

const QList<QSharedPointer<AbstractObject>>& ProjectManager::getObjects() const { return m_objects; }

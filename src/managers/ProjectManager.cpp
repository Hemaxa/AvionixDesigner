#include <QFile>
#include <QDomDocument>
#include <QTextStream>

#include "ProjectManager.h"
#include "ObjectsManager.h"
#include "RectangleObject.h"
#include "RotationObject.h"
#include "StaticGroupObject.h"
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
    
    // Открываем файл
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        emit logMessage(tr("ОШИБКА: Не удалось открыть файл"));
        return false;
    }
    
    // Парсим XML
    QDomDocument doc;
    QDomDocument::ParseResult result = doc.setContent(&file);
    if (!result) {
        emit logMessage(tr("ОШИБКА XML: %1").arg(result.errorMessage));
        file.close();
        return false;
    }
    file.close();
    
    // Получаем корневой элемент
    QDomElement root = doc.documentElement();
    
    // Читаем метаданные проекта
    m_projectName = root.attribute("name", "Untitled");
    m_canvasWidth = root.attribute("width", "640").toInt();
    m_canvasHeight = root.attribute("height", "480").toInt();
    
    // Парсим цвет фона
    QString bgStr = root.attribute("bgcolor", "#0");
    if (bgStr.startsWith("#")) {
        bool ok;
        uint colorVal = bgStr.mid(1).toUInt(&ok, 16);
        if (ok) {
            m_bgColor = QColor(colorVal & 0xFF, (colorVal >> 8) & 0xFF, (colorVal >> 16) & 0xFF);
        }
    }
    
    emit logMessage(tr("Проект: %1 (%2x%3)").arg(m_projectName).arg(m_canvasWidth).arg(m_canvasHeight));
    
    // Парсим схемы параметров
    QDomElement paramsEl = root.firstChildElement("parameters");
    QMap<QString, ParamSchema> schemas = ObjectsManager::instance()->parseSchemas(paramsEl);
    
    for (const QString &type : schemas.keys()) {
        emit logMessage(tr("Схема: %1 (%2 полей)").arg(type).arg(schemas[type].size()));
    }
    
    // Парсим объекты
    QDomElement objectsEl = root.firstChildElement("objects");
    QDomNode objNode = objectsEl.isNull() ? root.firstChild() : objectsEl.firstChild();
    
    //списки для отладочного дампа
    QList<QDomElement> debugElements;
    QStringList debugTypes;
    
    while (!objNode.isNull()) {
        QDomElement objEl = objNode.toElement();
        QString tagName = objEl.tagName();
        
        if (schemas.contains(tagName)) {
            QString hexInit = objEl.firstChildElement("init").text().trimmed();
            
            // Создаём объект через фабрику
            BaseObject *obj = ObjectsManager::instance()->createObject(tagName);
            
            if (obj && !hexInit.isEmpty()) {
                obj->parse(hexInit, schemas[tagName]);
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
    DebugDumper::dumpToFile(fileName, m_objects, schemas, debugElements, debugTypes);
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
    om->registerType("rectanglea", []() { return new RectangleObject(); });
    om->registerType("rectanglee", []() { return new RectangleObject(); });
    om->registerType("rotationobject", []() { return new RotationObject(); });
    om->registerType("staticgroup", []() { return new StaticGroupObject(); });
}

bool ProjectManager::saveToFile()
{
    if (m_filePath.isEmpty()) return false;
    
    // Переоткрываем оригинальный XML
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;
    
    QDomDocument doc;
    if (!doc.setContent(&file)) {
        file.close();
        return false;
    }
    file.close();
    
    QDomElement root = doc.documentElement();
    
    // Парсим схемы для определения типов объектов
    QDomElement objectsEl = root.firstChildElement("objects");
    QDomNode objNode = objectsEl.isNull() ? root.firstChild() : objectsEl.firstChild();
    
    int objIdx = 0;
    while (!objNode.isNull() && objIdx < m_objects.size()) {
        QDomElement objEl = objNode.toElement();
        if (!objEl.isNull()) {
            auto obj = m_objects[objIdx];
            
            // Удаляем старый элемент overrides если есть
            QDomElement oldOverrides = objEl.firstChildElement("overrides");
            if (!oldOverrides.isNull()) {
                objEl.removeChild(oldOverrides);
            }
            
            // Добавляем overrides с текущими значениями свойств
            QDomElement overrides = doc.createElement("overrides");
            const auto props = obj->getProperties();
            for (const auto &prop : props) {
                overrides.setAttribute(prop.first, prop.second);
            }
            objEl.appendChild(overrides);
            
            objIdx++;
        }
        objNode = objNode.nextSibling();
    }
    
    // Записываем XML обратно в файл
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

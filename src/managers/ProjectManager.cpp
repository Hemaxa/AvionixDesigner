#include <QFile>
#include <QDomDocument>
#include <QTextStream>
#include <QFileInfo>
#include <QImage>
#include <QSet>
#include <QStringEncoder>

#include "ProjectManager.h"
#include "ObjectsManager.h"
#include "RectangleObject.h"
#include "RotationObject.h"
#include "StaticGroupObject.h"
#include "AviaHorizonObject.h"
#include "BitParser.h"
#include "DebugDumper.h"

namespace {
struct SchemaFieldDef
{
    const char *name;
    int offset;
    int size;
};

using SchemaFieldList = QList<SchemaFieldDef>;

QString canonicalSchemaName(const QString &typeName)
{
    if (typeName == "rectangle_a")
        return "rectangle";
    if (typeName == "aviahorizont")
        return "aviagorizont";
    return typeName;
}

QString canonicalObjectTag(const QString &typeName)
{
    if (typeName == "rectangle_a")
        return "rectangle";
    if (typeName == "aviahorizont")
        return "aviagorizont";
    return typeName;
}

SchemaFieldList schemaFieldsFor(const QString &schemaName)
{
    if (schemaName == "rectangle") {
        return {
            {"enb", 0, 1},
            {"color", 1, 24},
            {"colorb", 25, 24},
            {"x0", 49, 12},
            {"y0", 61, 12},
            {"w", 73, 12},
            {"h", 85, 12},
            {"a", 97, 12},
            {"padding", 109, 41}
        };
    }

    if (schemaName == "rotationobject") {
        return {
            {"enb", 0, 1},
            {"color", 1, 24},
            {"xrot", 25, 12},
            {"yrot", 37, 12},
            {"top", 49, 12},
            {"left", 61, 12},
            {"bottom", 73, 12},
            {"right", 85, 12},
            {"sq", 97, 8},
            {"sin", 105, 18},
            {"cos", 123, 18},
            {"padding", 141, 9}
        };
    }

    if (schemaName == "staticgroup") {
        return {
            {"enb", 0, 1},
            {"color", 1, 24},
            {"x", 25, 12},
            {"y", 37, 12},
            {"w", 49, 12},
            {"h", 61, 12},
            {"addr", 73, 16},
            {"padding", 89, 61}
        };
    }

    if (schemaName == "aviagorizont") {
        return {
            {"enb", 0, 1},
            {"earth", 1, 24},
            {"sky", 25, 24},
            {"hline", 49, 24},
            {"width", 73, 4},
            {"xo", 77, 12},
            {"yo", 89, 12},
            {"sn", 101, 18},
            {"cs", 119, 18},
            {"padding", 137, 13}
        };
    }

    return {};
}

ParamSchema buildSchema(const QString &schemaName)
{
    ParamSchema schema;
    const auto fields = schemaFieldsFor(schemaName);
    for (const auto &field : fields) {
        schema.insert(QString::fromLatin1(field.name), {field.offset, field.size});
    }
    return schema;
}

int schemaBitLength(const ParamSchema &schema)
{
    int maxBit = 0;
    for (auto it = schema.constBegin(); it != schema.constEnd(); ++it) {
        maxBit = qMax(maxBit, it.value().offset + it.value().size);
    }
    return maxBit;
}

QString buildInitHex(const ParamSchema &schema, const QMap<QString, quint32> &params)
{
    const int hexLength = (schemaBitLength(schema) + 3) / 4;
    QString hexStr(hexLength, QLatin1Char('0'));

    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        if (schema.contains(it.key())) {
            const auto &info = schema[it.key()];
            BitParser::inject(hexStr, info.offset, info.size, it.value());
        }
    }

    return hexStr;
}

QString serializeMaskData(const QImage &image)
{
    QStringList values;
    values.reserve(image.width() * image.height());

    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const int alpha = image.pixelColor(x, y).alpha();
            const int level = qBound(0, qRound(alpha * 7.0 / 255.0), 7);
            values.append(QString::number(level));
        }
    }

    return values.join(", ");
}

QString serializeStaticGroupData(const StaticGroupObject *group)
{
    if (!group || group->states.isEmpty()) {
        return QStringLiteral("7");
    }

    int totalSize = 1;
    for (int i = 0; i < group->states.size(); ++i) {
        const GroupState &state = group->states[i];
        totalSize = qMax(totalSize, state.addr + qMax(1, state.w * state.h));
    }

    QVector<int> values(totalSize, 0);

    for (int i = 0; i < group->states.size(); ++i) {
        const GroupState &state = group->states[i];
        QImage image;
        if (i < group->maskImages.size() && !group->maskImages[i].isNull()) {
            image = group->maskImages[i];
        } else {
            image = QImage(qMax(1, state.w), qMax(1, state.h), QImage::Format_ARGB32);
            image.fill(QColor(255, 255, 255, 255));
        }

        const int width = qMin(image.width(), qMax(1, state.w));
        const int height = qMin(image.height(), qMax(1, state.h));
        int idx = qMax(0, state.addr);

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                if (idx >= values.size())
                    break;

                const int alpha = image.pixelColor(x, y).alpha();
                values[idx++] = qBound(0, qRound(alpha * 7.0 / 255.0), 7);
            }
        }
    }

    QStringList parts;
    parts.reserve(values.size());
    for (int value : values) {
        parts.append(QString::number(value));
    }
    return parts.join(", ");
}

QDomElement createObjectElement(QDomDocument &doc, const QString &tagName, const ParamSchema &schema, const QSharedPointer<BaseObject> &obj)
{
    QDomElement objEl = doc.createElement(tagName);

    if (auto staticGroup = dynamic_cast<StaticGroupObject*>(obj.data())) {
        objEl.setAttribute("nomber", staticGroup->groupNumber > 0 ? staticGroup->groupNumber : staticGroup->states.size());

        const int stateCount = qMax(1, staticGroup->states.size());
        for (int stateIndex = 0; stateIndex < stateCount; ++stateIndex) {
            QDomElement initEl = doc.createElement("init");
            initEl.setAttribute("index", stateIndex + 1);
            initEl.appendChild(doc.createTextNode(buildInitHex(schema, staticGroup->serializeState(stateIndex))));
            objEl.appendChild(initEl);
        }

        QDomElement dataEl = doc.createElement("data");
        dataEl.appendChild(doc.createTextNode(serializeStaticGroupData(staticGroup)));
        objEl.appendChild(dataEl);
        return objEl;
    }

    QDomElement initEl = doc.createElement("init");
    initEl.appendChild(doc.createTextNode(buildInitHex(schema, obj->serializeParams())));
    objEl.appendChild(initEl);

    if (auto rotation = dynamic_cast<RotationObject*>(obj.data())) {
        QDomElement dataEl = doc.createElement("data");
        QImage image = rotation->maskImage;
        if (image.isNull()) {
            image = QImage(1, 1, QImage::Format_ARGB32);
            image.fill(QColor(255, 255, 255, 255));
        }
        dataEl.setAttribute("width", image.width());
        dataEl.setAttribute("height", image.height());
        dataEl.appendChild(doc.createTextNode(serializeMaskData(image)));
        objEl.appendChild(dataEl);
    }

    return objEl;
}
}

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
    m_objectTags.clear();
    
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
    m_schemaAliases["aviahorizont"] = "aviagorizont";
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
        if (!m_schemas.contains(schemaName) && m_schemas.contains(tagName)) {
            schemaName = tagName;
        }
        
        if (m_schemas.contains(schemaName)) {
            QString hexInit = objEl.firstChildElement("init").text().trimmed();
            
            //создаем объект через фабрику
            BaseObject *obj = ObjectsManager::instance()->createObject(tagName);
            
            if (obj && !hexInit.isEmpty()) {
                obj->parse(hexInit, m_schemas[schemaName]);
                obj->parseExtraData(objEl);
                
                m_objects.append(QSharedPointer<BaseObject>(obj));
                m_objectTags.append(tagName);
                
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
    //om->registerType("rectangle_e", []() { return new RectangleObject(); });
    om->registerType("rotationobject", []() { return new RotationObject(); });
    om->registerType("staticgroup", []() { return new StaticGroupObject(); });
    om->registerType("aviagorizont", []() { return new AviaHorizonObject(); });
    om->registerType("aviahorizont", []() { return new AviaHorizonObject(); });
}

int ProjectManager::addObject(const QString &typeName)
{
    if (m_filePath.isEmpty()) {
        emit logMessage(tr("Сначала откройте XML файл проекта"));
        return -1;
    }

    BaseObject *rawObject = ObjectsManager::instance()->createObject(typeName);
    if (!rawObject) {
        emit logMessage(tr("Неизвестный тип объекта: %1").arg(typeName));
        return -1;
    }

    const int index = m_objects.size();
    const int offset = 24 * (index % 6);
    const double centerX = qBound(40.0, m_canvasWidth * 0.35 + offset, m_canvasWidth - 40.0);
    const double centerY = qBound(40.0, m_canvasHeight * 0.35 + offset, m_canvasHeight - 40.0);

    if (auto rect = dynamic_cast<RectangleObject*>(rawObject)) {
        rect->x = qMax(12.0, centerX - 60.0);
        rect->y = qMax(12.0, centerY - 40.0);
        rect->width = 120.0;
        rect->height = 80.0;
        rect->fillColor = QColor("#3BA8FF");
        rect->strokeColor = QColor("#EAF6FF");
        rect->strokeWidth = 2.0;
    }
    else if (auto horizon = dynamic_cast<AviaHorizonObject*>(rawObject)) {
        horizon->enabled = true;
        horizon->xCenter = centerX;
        horizon->yCenter = centerY;
        horizon->areaWidth = 220.0;
        horizon->areaHeight = 220.0;
        horizon->lineWidth = 4.0;
        horizon->earthColor = QColor("#C27D1B");
        horizon->skyColor = QColor("#4FCAF7");
        horizon->horizonLineColor = QColor("#C0FFFF");
        horizon->sinVal = 0.0;
        horizon->cosVal = 65536.0;
    }
    else if (auto rotation = dynamic_cast<RotationObject*>(rawObject)) {
        const int size = 36;
        rotation->xRot = centerX;
        rotation->yRot = centerY;
        rotation->left = -size / 2.0;
        rotation->top = -size / 2.0;
        rotation->right = size / 2.0;
        rotation->bottom = size / 2.0;
        rotation->sinVal = 0.0;
        rotation->cosVal = 65536.0;
        rotation->color = QColor("#F8FAFC");
        rotation->maskImage = QImage(size, size, QImage::Format_ARGB32);
        rotation->maskImage.fill(QColor(255, 255, 255, 255));
    }
    else if (auto staticGroup = dynamic_cast<StaticGroupObject*>(rawObject)) {
        GroupState state;
        state.x = qMax(12, qRound(centerX - 18.0));
        state.y = qMax(12, qRound(centerY - 18.0));
        state.w = 36;
        state.h = 36;
        state.addr = 0;
        state.color = QColor("#89D185");
        state.enabled = true;
        staticGroup->groupNumber = 1;
        staticGroup->states = {state};

        QImage mask(state.w, state.h, QImage::Format_ARGB32);
        mask.fill(QColor(255, 255, 255, 255));
        staticGroup->maskImages = {mask};
    }

    const QString schemaName = canonicalSchemaName(typeName);
    if (!m_schemas.contains(schemaName)) {
        ParamSchema schema = buildSchema(schemaName);
        if (!schema.isEmpty()) {
            m_schemas.insert(schemaName, schema);
        }
    }

    m_objects.append(QSharedPointer<BaseObject>(rawObject));
    m_objectTags.append(canonicalObjectTag(typeName));

    emit projectChanged();
    emit logMessage(tr("Добавлен объект: %1").arg(typeName));
    return m_objects.size() - 1;
}

bool ProjectManager::reorderObjects(const QList<int> &order)
{
    if (order.size() != m_objects.size())
        return false;

    QList<QSharedPointer<BaseObject>> reorderedObjects;
    QStringList reorderedTags;
    reorderedObjects.reserve(m_objects.size());
    reorderedTags.reserve(m_objectTags.size());

    QSet<int> seen;
    for (int index : order) {
        if (index < 0 || index >= m_objects.size() || seen.contains(index))
            return false;

        seen.insert(index);
        reorderedObjects.append(m_objects[index]);
        reorderedTags.append(index < m_objectTags.size() ? m_objectTags[index] : QString());
    }

    m_objects = reorderedObjects;
    m_objectTags = reorderedTags;
    emit projectChanged();
    emit logMessage(tr("Изменён порядок слоёв объектов"));
    return true;
}

bool ProjectManager::saveToFile(const QString &targetFile)
{
    if (m_filePath.isEmpty()) return false;
    QString outPath = targetFile.isEmpty() ? m_filePath : targetFile;
    
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
    QDomElement paramsEl = root.firstChildElement("parameters");
    if (paramsEl.isNull()) {
        paramsEl = doc.createElement("parameters");
        root.insertBefore(paramsEl, root.firstChild());
    }
    
    //обновляем bgcolor
    uint bgVal = (m_bgColor.blue() << 16) | (m_bgColor.green() << 8) | m_bgColor.red();
    root.setAttribute("bgcolor", QString("#%1").arg(bgVal, 0, 16));

    QSet<QString> requiredSchemas;
    for (const QString &tagName : m_objectTags) {
        requiredSchemas.insert(m_schemaAliases.value(tagName, canonicalSchemaName(tagName)));
    }

    for (const QString &schemaName : requiredSchemas) {
        if (!m_schemas.contains(schemaName)) {
            ParamSchema schema = buildSchema(schemaName);
            if (!schema.isEmpty()) {
                m_schemas.insert(schemaName, schema);
            }
        }

        if (paramsEl.firstChildElement(schemaName).isNull()) {
            const auto fields = schemaFieldsFor(schemaName);
            if (!fields.isEmpty()) {
                QDomElement schemaEl = doc.createElement(schemaName);
                for (const auto &field : fields) {
                    QDomElement fieldEl = doc.createElement(QString::fromLatin1(field.name));
                    fieldEl.setAttribute("offset", field.offset);
                    fieldEl.setAttribute("size", field.size);
                    schemaEl.appendChild(fieldEl);
                }
                paramsEl.appendChild(schemaEl);
            }
        }
    }
    
    QDomElement objectsEl = root.firstChildElement("objects");
    if (objectsEl.isNull()) {
        objectsEl = doc.createElement("objects");
        root.appendChild(objectsEl);
    }
    while (!objectsEl.firstChild().isNull()) {
        objectsEl.removeChild(objectsEl.firstChild());
    }

    for (int objIdx = 0; objIdx < m_objects.size(); ++objIdx) {
        const QString tagName = objIdx < m_objectTags.size() ? m_objectTags[objIdx] : QString();
        const QString schemaName = m_schemaAliases.value(tagName, canonicalSchemaName(tagName));
        if (tagName.isEmpty() || !m_schemas.contains(schemaName))
            continue;

        objectsEl.appendChild(createObjectElement(doc, tagName, m_schemas[schemaName], m_objects[objIdx]));
    }
    
    //записываем XML обратно в файл
    QFile outFile(outPath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    
    const QString xmlText = doc.toString(4);
    QStringEncoder encoder("windows-1251");
    QByteArray encodedXml = encoder(xmlText);
    if (encodedXml.isEmpty() && !xmlText.isEmpty()) {
        encodedXml = xmlText.toUtf8();
    }
    outFile.write(encodedXml);
    outFile.close();
    
    if (!targetFile.isEmpty() && targetFile != m_filePath) {
        m_filePath = targetFile;
        QFileInfo fi(targetFile);
        m_projectName = fi.baseName();
        emit projectLoaded(); // Обновляем заголовок и UI
    }
    
    emit logMessage(tr("Проект сохранён: %1").arg(outPath));
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

#include <QFile>
#include <QDomDocument>
#include <QTextStream>
#include <QFileInfo>
#include <QImage>
#include <QSet>
#include <QStringEncoder>

#include "ProjectManager.h"
#include "EditorProjectDocument.h"
#include "FpgaSchemaRegistry.h"
#include "ObjectsManager.h"
#include "RectangleObject.h"
#include "DashedLineObject.h"
#include "RibbonScaleObject.h"
#include "RotationObject.h"
#include "StaticGroupObject.h"
#include "AviaHorizonObject.h"
#include "TextObject.h"
#include "BitParser.h"
#include "DebugDumper.h"

namespace {
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

QString formatProjectColor(const QColor &color)
{
    const uint bgVal = (color.blue() << 16) | (color.green() << 8) | color.red();
    return QString("#%1").arg(bgVal, 0, 16);
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

QDomDocument buildProjectDocument(const QString &projectName,
                                  int canvasWidth,
                                  int canvasHeight,
                                  const QColor &backgroundColor,
                                  const QMap<QString, ParamSchema> &schemas,
                                  const QMap<QString, QString> &schemaAliases,
                                  const QList<QSharedPointer<BaseObject>> &objects,
                                  const QStringList &objectTags)
{
    QDomDocument doc;
    doc.appendChild(doc.createProcessingInstruction("xml", "version='1.0' encoding='windows-1251'"));

    QDomElement root = doc.createElement("project");
    root.setAttribute("name", projectName.isEmpty() ? QStringLiteral("Untitled") : projectName);
    root.setAttribute("width", canvasWidth);
    root.setAttribute("height", canvasHeight);
    root.setAttribute("bgcolor", formatProjectColor(backgroundColor));
    doc.appendChild(root);

    QDomElement paramsEl = doc.createElement("parameters");
    root.appendChild(paramsEl);

    auto *registry = FpgaSchemaRegistry::instance();
    QSet<QString> requiredSchemas;
    for (const QString &schemaName : registry->defaultSchemaNames()) {
        requiredSchemas.insert(schemaName);
    }
    for (const QString &tagName : objectTags) {
        requiredSchemas.insert(schemaAliases.value(tagName, registry->canonicalSchemaName(tagName)));
    }

    const QStringList orderedSchemas = registry->orderedSchemaNames();
    for (const QString &schemaName : orderedSchemas) {
        if (!requiredSchemas.contains(schemaName))
            continue;

        QDomElement schemaEl = doc.createElement(schemaName);
        const auto fields = registry->fieldsForSchema(schemaName);
        for (const auto &field : fields) {
            QDomElement fieldEl = doc.createElement(field.name);
            fieldEl.setAttribute("offset", field.offset);
            fieldEl.setAttribute("size", field.size);
            schemaEl.appendChild(fieldEl);
        }
        paramsEl.appendChild(schemaEl);
    }

    for (const QString &schemaName : requiredSchemas) {
        if (orderedSchemas.contains(schemaName))
            continue;

        if (!schemas.contains(schemaName))
            continue;

        QDomElement schemaEl = doc.createElement(schemaName);
        const auto fields = registry->fieldsForSchema(schemaName);
        for (const auto &field : fields) {
            QDomElement fieldEl = doc.createElement(field.name);
            fieldEl.setAttribute("offset", field.offset);
            fieldEl.setAttribute("size", field.size);
            schemaEl.appendChild(fieldEl);
        }
        paramsEl.appendChild(schemaEl);
    }

    QDomElement objectsEl = doc.createElement("objects");
    root.appendChild(objectsEl);

    for (int objIdx = 0; objIdx < objects.size(); ++objIdx) {
        const QString tagName = objIdx < objectTags.size() ? objectTags[objIdx] : QString();
        const QString schemaName = schemaAliases.value(tagName, registry->canonicalSchemaName(tagName));
        if (tagName.isEmpty() || !schemas.contains(schemaName))
            continue;

        objectsEl.appendChild(createObjectElement(doc, tagName, schemas[schemaName], objects[objIdx]));
    }

    return doc;
}
}

ProjectManager::ProjectManager() : m_document(new EditorProjectDocument()) {}

ProjectManager* ProjectManager::instance()
{
    static ProjectManager s_instance;
    return &s_instance;
}

bool ProjectManager::loadFromFile(const QString &fileName)
{
    auto *registry = FpgaSchemaRegistry::instance();
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
    m_document->setProjectName(root.attribute("name", "Untitled"));
    m_document->setCanvasSize(root.attribute("width", "640").toInt(), root.attribute("height", "480").toInt());
    m_document->setBackgroundColor(Qt::black);
    
    //парсим цвет фона
    QString bgStr = root.attribute("bgcolor", "#0");
    if (bgStr.startsWith("#")) {
        bool ok;
        uint colorVal = bgStr.mid(1).toUInt(&ok, 16);
        if (ok) {
            m_document->setBackgroundColor(QColor(colorVal & 0xFF, (colorVal >> 8) & 0xFF, (colorVal >> 16) & 0xFF));
        }
    }
    
    emit logMessage(tr("Проект: %1 (%2x%3)").arg(m_document->projectName()).arg(m_document->canvasWidth()).arg(m_document->canvasHeight()));
    
    //парсим схемы параметров и сохраняем для использования при сохранении
    QDomElement paramsEl = root.firstChildElement("parameters");
    m_schemas = ObjectsManager::instance()->parseSchemas(paramsEl);
    
    //маппинг тегов на схемы параметров (для тегов без собственной схемы)
    m_schemaAliases = registry->defaultSchemaAliases();
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

bool ProjectManager::createNewProject(const QString &projectName, int width, int height, const QColor &backgroundColor, const QString &filePath)
{
    auto *registry = FpgaSchemaRegistry::instance();
    m_document->setProjectName(projectName);
    m_document->setCanvasSize(width, height);
    m_document->setBackgroundColor(backgroundColor);
    m_filePath = filePath;

    m_objects.clear();
    m_objectTags.clear();
    m_schemaAliases = registry->defaultSchemaAliases();

    m_schemas.clear();
    const QStringList defaultSchemas = registry->defaultSchemaNames();
    for (const QString &schemaName : defaultSchemas) {
        m_schemas.insert(schemaName, registry->buildSchema(schemaName));
    }

    emit logMessage(tr("Создан новый проект: %1 (%2x%3)").arg(m_document->projectName()).arg(m_document->canvasWidth()).arg(m_document->canvasHeight()));
    emit projectLoaded();
    emit projectChanged();
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
    om->registerType("dashed_line", []() { return new DashedLineObject(); });
    om->registerType("RibonScale", []() { return new RibbonScaleObject(); });
    om->registerType("ribonscale", []() { return new RibbonScaleObject(); });
    om->registerType("rotationobject", []() { return new RotationObject(); });
    om->registerType("staticgroup", []() { return new StaticGroupObject(); });
    om->registerType("aviagorizont", []() { return new AviaHorizonObject(); });
    om->registerType("aviahorizont", []() { return new AviaHorizonObject(); });
    om->registerType("text", []() { return new TextObject(); });
}

int ProjectManager::addObject(const QString &typeName)
{
    auto *registry = FpgaSchemaRegistry::instance();

    if (!m_document->hasCanvas()) {
        emit logMessage(tr("Сначала откройте или создайте проект"));
        return -1;
    }

    BaseObject *rawObject = ObjectsManager::instance()->createObject(typeName);
    if (!rawObject) {
        emit logMessage(tr("Неизвестный тип объекта: %1").arg(typeName));
        return -1;
    }

    const int index = m_objects.size();
    const int offset = 24 * (index % 6);
    const double centerX = qBound(40.0, m_document->canvasWidth() * 0.35 + offset, m_document->canvasWidth() - 40.0);
    const double centerY = qBound(40.0, m_document->canvasHeight() * 0.35 + offset, m_document->canvasHeight() - 40.0);

    if (auto rect = dynamic_cast<RectangleObject*>(rawObject)) {
        rect->x = qMax(12.0, centerX - 60.0);
        rect->y = qMax(12.0, centerY - 40.0);
        rect->width = 120.0;
        rect->height = 80.0;
        rect->fillColor = QColor("#3BA8FF");
        rect->strokeColor = QColor("#EAF6FF");
        rect->strokeWidth = 2.0;
    }
    else if (auto dashedLine = dynamic_cast<DashedLineObject*>(rawObject)) {
        dashedLine->x0 = qMax(12.0, centerX - 70.0);
        dashedLine->y0 = centerY;
        dashedLine->x1 = qMin(static_cast<double>(m_document->canvasWidth() - 12), centerX + 70.0);
        dashedLine->y1 = centerY + 42.0;
        dashedLine->color = QColor("#9EDCFF");
        dashedLine->lineWidth = 3;
        dashedLine->dashPeriod = 16;
        dashedLine->dashLength = 8;
        dashedLine->dashPhase = 0;
    }
    else if (auto ribbon = dynamic_cast<RibbonScaleObject*>(rawObject)) {
        ribbon->left = qMax(12.0, centerX - 48.0);
        ribbon->right = qMin(static_cast<double>(m_document->canvasWidth() - 12), centerX + 48.0);
        ribbon->top = qMax(12.0, centerY - 90.0);
        ribbon->bottom = qMin(static_cast<double>(m_document->canvasHeight() - 12), centerY + 90.0);
        ribbon->lineWidth = 2;
        ribbon->period = 24;
        ribbon->yStart = ribbon->top + 12.0;
        ribbon->color = QColor("#8DE1FF");
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
    else if (auto textObject = dynamic_cast<TextObject*>(rawObject)) {
        if (!textObject->states.isEmpty()) {
            textObject->states[0].x = qMax(12, qRound(centerX - textObject->states[0].w / 2.0));
            textObject->states[0].y = qMax(12, qRound(centerY - textObject->states[0].h / 2.0));
        }
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

    const QString schemaName = registry->canonicalSchemaName(typeName);
    if (!m_schemas.contains(schemaName)) {
        ParamSchema schema = registry->buildSchema(schemaName);
        if (!schema.isEmpty()) {
            m_schemas.insert(schemaName, schema);
        }
    }

    m_objects.append(QSharedPointer<BaseObject>(rawObject));
    m_objectTags.append(registry->canonicalObjectTag(typeName));

    emit projectChanged();
    emit logMessage(tr("Добавлен объект: %1").arg(typeName));
    return m_objects.size() - 1;
}

bool ProjectManager::removeObject(int index)
{
    if (index < 0 || index >= m_objects.size())
        return false;

    m_objects.removeAt(index);
    if (index < m_objectTags.size()) {
        m_objectTags.removeAt(index);
    }

    emit projectChanged();
    emit logMessage(tr("Удалён объект #%1").arg(index + 1));
    return true;
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

bool ProjectManager::alignObject(int index, ObjectAlignment alignment)
{
    if (index < 0 || index >= m_objects.size() || !m_document->hasCanvas())
        return false;

    const auto object = m_objects[index];
    const QRectF bounds = object->getBoundingRect();
    if (bounds.isEmpty())
        return false;

    double dx = 0.0;
    double dy = 0.0;
    switch (alignment) {
    case ObjectAlignment::Left:
        dx = -bounds.left();
        break;
    case ObjectAlignment::HCenter:
        dx = m_document->canvasWidth() / 2.0 - bounds.center().x();
        break;
    case ObjectAlignment::Right:
        dx = m_document->canvasWidth() - bounds.right();
        break;
    case ObjectAlignment::Top:
        dy = -bounds.top();
        break;
    case ObjectAlignment::VCenter:
        dy = m_document->canvasHeight() / 2.0 - bounds.center().y();
        break;
    case ObjectAlignment::Bottom:
        dy = m_document->canvasHeight() - bounds.bottom();
        break;
    }

    object->moveBy(dx, dy);
    emit projectChanged();
    emit logMessage(tr("Выполнено выравнивание объекта"));
    return true;
}

bool ProjectManager::saveToFile(const QString &targetFile)
{
    const QString outPath = targetFile.isEmpty() ? m_filePath : targetFile;
    if (outPath.isEmpty())
        return false;

    const QDomDocument doc = buildProjectDocument(
        m_document->projectName(),
        m_document->canvasWidth(),
        m_document->canvasHeight(),
        m_document->backgroundColor(),
        m_schemas,
        m_schemaAliases,
        m_objects,
        m_objectTags
    );
    
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
QString ProjectManager::getProjectName() const { return m_document->projectName(); }
int ProjectManager::getCanvasWidth() const { return m_document->canvasWidth(); }
int ProjectManager::getCanvasHeight() const { return m_document->canvasHeight(); }
QColor ProjectManager::getBackgroundColor() const { return m_document->backgroundColor(); }
QString ProjectManager::getFilePath() const { return m_filePath; }
const QList<QSharedPointer<BaseObject>>& ProjectManager::getObjects() const { return m_objects; }

//сеттеры
void ProjectManager::setBackgroundColor(const QColor &color)
{
    if (m_document->backgroundColor() != color) {
        m_document->setBackgroundColor(color);
        emit projectChanged();
    }
}

void ProjectManager::setCanvasSize(int width, int height)
{
    const int clampedWidth = qMax(1, width);
    const int clampedHeight = qMax(1, height);
    if (m_document->canvasWidth() != clampedWidth || m_document->canvasHeight() != clampedHeight) {
        m_document->setCanvasSize(clampedWidth, clampedHeight);
        emit projectChanged();
    }
}

// ProjectManager - менеджер проекта, связывающий внутреннюю модель редактора с экспортом в XML.

#include <QLineF>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSet>
#include <QImageReader>
#include <QSvgRenderer>
#include "ApplicationSettings.h"
#include <QtMath>

#include "ProjectManager.h"
#include "ObjectCloner.h"
#include "CanvasGeometry.h"
#include "ProjectXmlFile.h"
#include "fpga/compiler/FpgaXmlExporter.h"
#include "fpga/compiler/ImageResourceBuilder.h"
#include "EditorProjectXml.h"
#include "EditorProjectDocument.h"
#include "FpgaSchemaRegistry.h"
#include "ObjectsManager.h"
#include "RectangleObject.h"
#include "DashedLineObject.h"
#include "RibbonScaleObject.h"
#include "RotationObject.h"
#include "StaticGroupObject.h"
#include "ImageObject.h"
#include "AviaHorizonObject.h"
#include "ThreeDViewObject.h"
#include <QScopedValueRollback>
#include "TextObject.h"
#include "fpga/model/FpgaCompiledDocument.h"
#include "fpga/compiler/FpgaResourceLimits.h"
#include "NumericExpression.h"
#include "ObjectReflection.h"

#include <algorithm>

ProjectManager::ProjectManager()
{
    resetEditorLayers();
    connect(this, &ProjectManager::projectLoaded, this, [this]() { ++m_sceneRevision; });
    connect(this, &ProjectManager::projectChanged, this, [this]() {
        if (!m_aircraftStateChange) ++m_sceneRevision;
    });
    reloadGlobalSettings();
}

ProjectManager* ProjectManager::instance()
{
    static ProjectManager s_instance;
    return &s_instance;
}

QString ProjectManager::lastErrorMessage() const
{
    return m_lastErrorMessage;
}

bool ProjectManager::loadFromFile(const QString &fileName)
{
    emit logMessage(tr("Загрузка: %1").arg(fileName));

    QDomDocument doc;
    QString readError;
    if (!ProjectXmlFile::read(fileName, &doc, &readError)) {
        m_lastErrorMessage = readError;
        emit logMessage(tr("ОШИБКА: %1").arg(readError));
        return false;
    }

    const auto sourceDocument = FpgaCompiledDocument::fromDomDocument(doc, &m_lastErrorMessage);
    if (!sourceDocument.isValid()) {
        emit logMessage(tr("ОШИБКА: %1").arg(m_lastErrorMessage));
        return false;
    }
    const auto editor = doc.documentElement().firstChildElement("editor");
    if (editor.isNull()) {
        m_lastErrorMessage = tr("В XML отсутствует обязательный раздел <editor> с настройками проекта.");
        return false;
    }
    EditorProjectXml::State state;
    if (!EditorProjectXml::read(editor, &state, &m_lastErrorMessage)) {
        emit logMessage(tr("ОШИБКА данных редактора: %1").arg(m_lastErrorMessage));
        return false;
    }
    if (state.document.canvasWidth() != sourceDocument.width || state.document.canvasHeight() != sourceDocument.height
        || state.document.projectName() != sourceDocument.projectName || state.document.backgroundColor().rgb() != sourceDocument.backgroundColor.rgb()) {
        m_lastErrorMessage = tr("Настройки <editor>/<workspace> не совпадают с заголовком проекта.");
        return false;
    }
    m_document = state.document;
    m_objects = state.objects; m_objectTags = state.objectTags;
    m_groups = state.groups; m_nextGroupId = state.nextGroupId;
    m_editorLayers = state.layers; m_nextLayerId = state.nextLayerId;
    m_activeLayerId = state.activeLayerId; m_showAllLayers = state.showAllLayers;
    m_showGrid = state.showGrid; m_snapToCanvas = state.snapToCanvas; m_snapToGrid = state.snapToGrid; m_snapToObjects = state.snapToObjects;
    m_gridStep = state.gridStep; m_gridColor = state.gridColor;
    m_snapCanvasGuideColor = state.canvasGuideColor; m_snapGridGuideColor = state.gridGuideColor; m_snapObjectGuideColor = state.objectGuideColor;
    m_editorViewState = state.view;
    m_fpgaConfiguration = state.fpgaConfiguration;
    m_fonts = sourceDocument.fonts;
    m_schemas = sourceDocument.schemas;
    m_filePath = fileName;
    clearHistory();
    m_lastErrorMessage.clear();
    emit projectLoaded();
    emit editorViewStateChanged();
    emit editorLayersChanged();
    emit editorLayerViewChanged();
    emit logMessage(tr("Проект восстановлен: %1 объектов, %2 слоёв, %3 групп.").arg(m_objects.size()).arg(m_editorLayers.size()).arg(m_groups.size()));
    return true;
}

bool ProjectManager::createNewProject(const QString &projectName, int width, int height, const QColor &backgroundColor, const QString &filePath)
{
    auto *registry = FpgaSchemaRegistry::instance();
    m_lastErrorMessage.clear();
    m_document.setProjectName(projectName);
    m_document.setCanvasSize(width, height);
    m_document.setBackgroundColor(backgroundColor);
    m_filePath = filePath;
    m_showGrid = false;
    m_snapToCanvas = true;
    m_snapToGrid = false;
    m_snapToObjects = false;
    m_editorViewState = {};
    m_fpgaConfiguration = {};
    readGlobalWorkspaceSettings();

    m_objects.clear();
    m_objectTags.clear();
    m_groups.clear();
    m_nextGroupId = 1;
    resetEditorLayers();
    clearHistory();
    m_fonts.clear();

    m_schemas.clear();
    const QStringList defaultSchemas = registry->defaultSchemaNames();
    for (const QString &schemaName : defaultSchemas) {
        m_schemas.insert(schemaName, registry->buildSchema(schemaName));
    }

    emit logMessage(tr("Создан новый проект: %1 (%2x%3)").arg(m_document.projectName()).arg(m_document.canvasWidth()).arg(m_document.canvasHeight()));
    emit projectLoaded();
    emit editorViewStateChanged();
    emit projectChanged();
    return true;
}

ProjectManager::ProjectSnapshot ProjectManager::captureSnapshot() const
{
    return {ObjectCloner::cloneScene(m_objects), m_objectTags, m_groups, m_nextGroupId,
            m_editorLayers, m_nextLayerId, m_activeLayerId, m_showAllLayers};
}

void ProjectManager::restoreSnapshot(const ProjectSnapshot &snapshot)
{
    m_objects = ObjectCloner::cloneScene(snapshot.objects);
    m_objectTags = snapshot.objectTags;
    m_groups = snapshot.groups;
    m_nextGroupId = snapshot.nextGroupId;
    m_editorLayers = snapshot.layers;
    m_nextLayerId = snapshot.nextLayerId;
    m_activeLayerId = snapshot.activeLayerId;
    m_showAllLayers = snapshot.showAllLayers;
    emit editorLayerObjectsChanged();
    emit editorLayersChanged();
    emit editorLayerViewChanged();
}

void ProjectManager::recordHistory()
{
    m_undoStack.append(captureSnapshot());
    m_redoStack.clear();
    constexpr int maxHistoryDepth = 80;
    while (m_undoStack.size() > maxHistoryDepth)
        m_undoStack.removeFirst();
}

void ProjectManager::clearHistory()
{
    m_undoStack.clear();
    m_redoStack.clear();
    m_clipboardObjects.clear();
    m_clipboardTags.clear();
    m_clipboardGroups.clear();
}

void ProjectManager::insertObjectAtFront(BaseObject *object, const QString &tagName)
{
    object->setEditorLayerId(m_activeLayerId);
    m_objects.prepend(QSharedPointer<BaseObject>(object));
    m_objectTags.prepend(tagName);
    for (ObjectGroup &group : m_groups) {
        for (int &member : group.members)
            ++member;
    }
}

bool ProjectManager::undo()
{
    if (m_undoStack.isEmpty())
        return false;

    m_redoStack.append(captureSnapshot());
    const ProjectSnapshot snapshot = m_undoStack.takeLast();
    restoreSnapshot(snapshot);
    emit projectChanged();
    emit logMessage(tr("Отмена действия"));
    return true;
}

bool ProjectManager::redo()
{
    if (m_redoStack.isEmpty())
        return false;

    m_undoStack.append(captureSnapshot());
    const ProjectSnapshot snapshot = m_redoStack.takeLast();
    restoreSnapshot(snapshot);
    emit projectChanged();
    emit logMessage(tr("Повтор действия"));
    return true;
}

bool ProjectManager::canUndo() const
{
    return !m_undoStack.isEmpty();
}

bool ProjectManager::canRedo() const
{
    return !m_redoStack.isEmpty();
}

// метод регистрации стандартых типов объектов (получение словаря поддерживаемых объектов)
void ProjectManager::registerStandardTypes()
{
    // создание экземпляра менеджера объектов
    auto om = ObjectsManager::instance();

    // регистрируем типы объектов
    om->registerType("rectangle", []() { return new RectangleObject(); });
    om->registerType("rectangle_a", []() { return new RectangleObject(); });
    om->registerType("dashline", []() { return new DashedLineObject(); });
    om->registerType("RibonScale", []() { return new RibbonScaleObject(); });
    om->registerType("rotationobject", []() { return new RotationObject(); });
    om->registerType("staticgroup", []() { return new StaticGroupObject(); });
    om->registerType("aviahorizont", []() { return new AviaHorizonObject(); });
    om->registerType("layer3d", []() { return new ThreeDViewObject(); });
    om->registerType("text", []() { return new TextObject(); });
}

int ProjectManager::addObject(const QString &typeName)
{
    auto *registry = FpgaSchemaRegistry::instance();

    if (!m_document.hasCanvas()) {
        emit logMessage(tr("Сначала откройте или создайте проект"));
        return -1;
    }

    if (typeName == QStringLiteral("layer3d")) {
        for (int i = 0; i < m_objects.size(); ++i) {
            if (dynamic_cast<ThreeDViewObject*>(m_objects[i].data())) {
                emit logMessage(tr("3D-вид уже добавлен в сцену"));
                return i;
            }
        }
    }

    BaseObject *rawObject = ObjectsManager::instance()->createObject(typeName);
    if (!rawObject) {
        emit logMessage(tr("Неизвестный тип объекта: %1").arg(typeName));
        return -1;
    }

    const int offset = 24 * (m_objects.size() % 6);
    const double marginX = qMin(40.0, m_document.canvasWidth() * 0.5);
    const double marginY = qMin(40.0, m_document.canvasHeight() * 0.5);
    const double centerX = qBound(marginX, m_document.canvasWidth() * 0.35 + offset,
                                   m_document.canvasWidth() - marginX);
    const double centerY = qBound(marginY, m_document.canvasHeight() * 0.35 + offset,
                                   m_document.canvasHeight() - marginY);

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
        dashedLine->x1 = qMin(static_cast<double>(m_document.canvasWidth() - 12), centerX + 70.0);
        dashedLine->y1 = centerY + 42.0;
        dashedLine->color = QColor("#9EDCFF");
        dashedLine->lineWidth = 3;
        dashedLine->dashPeriod = 16;
        dashedLine->dashLength = 8;
        dashedLine->dashPhase = 0;
    }
    else if (auto ribbon = dynamic_cast<RibbonScaleObject*>(rawObject)) {
        ribbon->left = qMax(12.0, centerX - 48.0);
        ribbon->right = qMin(static_cast<double>(m_document.canvasWidth() - 12), centerX + 48.0);
        ribbon->top = qMax(12.0, centerY - 90.0);
        ribbon->bottom = qMin(static_cast<double>(m_document.canvasHeight() - 12), centerY + 90.0);
        ribbon->lineWidth = 2;
        ribbon->period = 24;
        ribbon->yStart = ribbon->top + 12.0;
        ribbon->color = QColor("#8DE1FF");
    }
    else if (auto horizon = dynamic_cast<AviaHorizonObject*>(rawObject)) {
        horizon->enabled = true;
        horizon->xCenter = m_document.canvasWidth() / 2.0;
        horizon->yCenter = m_document.canvasHeight() / 2.0;
        horizon->areaWidth = m_document.canvasWidth();
        horizon->areaHeight = m_document.canvasHeight();
        horizon->lineWidth = 4.0;
        horizon->earthColor = QColor("#C27D1B");
        horizon->skyColor = QColor("#4FCAF7");
        horizon->horizonLineColor = QColor("#C0FFFF");
        horizon->sinVal = 0.0;
        horizon->cosVal = 65536.0;
    }
    else if (auto threeDView = dynamic_cast<ThreeDViewObject*>(rawObject)) {
        threeDView->setViewVisible(true);
        threeDView->setExportEnabled(true);
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
        if (!m_fonts.isEmpty()) {
            const FpgaFont font = m_fonts.constBegin().value();
            const QString characters = font.orderedCharacters();
            textObject->text = characters.isEmpty() ? QStringLiteral(" ") : QString(characters.front());
            textObject->setFontAtlas(font, true);
        } else {
            textObject->pixelSize = 14;
            textObject->fontFamily = QStringLiteral("Arial");
        }
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

    const QString schemaName = typeName;
    if (!m_schemas.contains(schemaName)) {
        ParamSchema schema = registry->buildSchema(schemaName);
        if (!schema.isEmpty()) {
            m_schemas.insert(schemaName, schema);
        }
    }

    recordHistory();
    insertObjectAtFront(rawObject, typeName);

    emit projectChanged();
    emit logMessage(tr("Добавлен объект: %1").arg(typeName));
    return 0;
}

int ProjectManager::addObject(const QString &typeName, double x, double y)
{
    const int index = addObject(typeName);
    if (index < 0)
        return index;

    auto obj = m_objects[index];
    if (dynamic_cast<AviaHorizonObject*>(obj.data()) || dynamic_cast<ThreeDViewObject*>(obj.data())) {
        emit projectChanged();
        return index;
    }

    // Перемещаем объект так, чтобы его центр оказался в точке (x, y)
    const QRectF bounds = obj->getBoundingRect();
    const double dx = x - bounds.center().x();
    const double dy = y - bounds.center().y();
    obj->moveBy(dx, dy);
    CanvasGeometry::keepInside(obj.data(), m_document.canvasWidth(), m_document.canvasHeight());

    emit projectChanged();
    return index;
}

bool ProjectManager::removeObjects(const QList<int> &indexes)
{
    QList<int> normalized;
    QSet<int> seen;
    for (int index : indexes) {
        if (index < 0 || index >= m_objects.size() || seen.contains(index))
            continue;
        seen.insert(index);
        normalized.append(index);
    }

    for (int i = 0; i < m_objects.size(); ++i) {
        const auto *copy = dynamic_cast<SymmetryCopyObject *>(m_objects[i].data());
        if (copy && seen.contains(m_objects.indexOf(copy->source())) && !seen.contains(i)) {
            seen.insert(i); normalized.append(i);
        }
    }
    if (normalized.isEmpty())
        return false;

    recordHistory();
    std::sort(normalized.begin(), normalized.end(), std::greater<int>());
    for (int index : normalized) {
        m_objects.removeAt(index);
        if (index < m_objectTags.size())
            m_objectTags.removeAt(index);
    }

    for (ObjectGroup &group : m_groups) {
        QList<int> updatedMembers;
        for (int member : group.members) {
            if (seen.contains(member))
                continue;
            int shift = 0;
            for (int removed : normalized) {
                if (removed < member)
                    ++shift;
            }
            updatedMembers.append(member - shift);
        }
        group.members = updatedMembers;
    }
    m_groups.erase(std::remove_if(m_groups.begin(), m_groups.end(), [](const ObjectGroup &group) {
        return group.members.size() < 2;
    }), m_groups.end());

    emit projectChanged();
    emit logMessage(tr("Удалено объектов: %1").arg(normalized.size()));
    return true;
}

bool ProjectManager::reorderObjects(const QList<int> &order)
{
    if (order.size() != m_objects.size())
        return false;

    bool changed = false;
    for (int i = 0; i < order.size(); ++i) {
        if (order[i] != i) {
            changed = true;
            break;
        }
    }
    if (!changed)
        return true;

    QList<QSharedPointer<BaseObject>> reorderedObjects;
    QStringList reorderedTags;
    reorderedObjects.reserve(m_objects.size());
    reorderedTags.reserve(m_objectTags.size());

    QSet<int> seen;
    QMap<int, int> oldToNew;
    for (int index : order) {
        if (index < 0 || index >= m_objects.size() || seen.contains(index))
            return false;

        seen.insert(index);
        oldToNew.insert(index, reorderedObjects.size());
        reorderedObjects.append(m_objects[index]);
        reorderedTags.append(index < m_objectTags.size() ? m_objectTags[index] : QString());
    }

    recordHistory();
    m_objects = reorderedObjects;
    m_objectTags = reorderedTags;
    for (ObjectGroup &group : m_groups) {
        QList<int> updatedMembers;
        for (int member : group.members) {
            if (oldToNew.contains(member))
                updatedMembers.append(oldToNew.value(member));
        }
        std::sort(updatedMembers.begin(), updatedMembers.end());
        group.members = updatedMembers;
    }
    emit projectChanged();
    emit logMessage(tr("Изменён порядок слоёв объектов"));
    return true;
}

bool ProjectManager::sendObjectsToFront(const QList<int> &indexes)
{
    QSet<int> selected;
    for (int index : indexes) {
        if (index >= 0 && index < m_objects.size())
            selected.insert(index);
    }
    if (selected.isEmpty())
        return false;

    QList<int> order;
    for (int i = 0; i < m_objects.size(); ++i) {
        if (selected.contains(i))
            order.append(i);
    }
    for (int i = 0; i < m_objects.size(); ++i) {
        if (!selected.contains(i))
            order.append(i);
    }
    return reorderObjects(order);
}

bool ProjectManager::sendObjectsToBack(const QList<int> &indexes)
{
    QSet<int> selected;
    for (int index : indexes) {
        if (index >= 0 && index < m_objects.size())
            selected.insert(index);
    }
    if (selected.isEmpty())
        return false;

    QList<int> order;
    for (int i = 0; i < m_objects.size(); ++i) {
        if (!selected.contains(i))
            order.append(i);
    }
    for (int i = 0; i < m_objects.size(); ++i) {
        if (selected.contains(i))
            order.append(i);
    }
    return reorderObjects(order);
}

int ProjectManager::mergeStaticGroups(const QList<int> &indexes, QString *errorMessage)
{
    if (errorMessage) errorMessage->clear();
    auto fail = [errorMessage](const QString &message) {
        if (errorMessage) *errorMessage = message;
        return -1;
    };
    QList<int> selected;
    for (int index : indexes) {
        if (index < 0 || index >= m_objects.size())
            return fail(tr("В выделении есть отсутствующий объект."));
        if (!selected.contains(index)) selected.append(index);
    }
    std::sort(selected.begin(), selected.end());
    if (selected.size() < 2)
        return fail(tr("Выберите минимум два статических изображения или статические группы."));

    auto merged = QSharedPointer<StaticGroupObject>::create();
    merged->setEditorLayerId(m_objects[selected.first()]->editorLayerId());
    const bool exportEnabled = m_objects[selected.first()]->isExportEnabled();
    int nextAddress = 0;
    bool resizeLocked = false;
    // Внутри группы последний init расположен на переднем плане, как при экспорте.
    for (auto it = selected.crbegin(); it != selected.crend(); ++it) {
        const auto source = m_objects[*it];
        for (const auto &object : m_objects) {
            const auto *copy = dynamic_cast<SymmetryCopyObject *>(object.data());
            if (copy && copy->source() == source)
                return fail(tr("Сначала сделайте зависимые симметричные копии независимыми."));
        }
        if (source->editorLayerId() != merged->editorLayerId())
            return fail(tr("Для объединения в staticgroup переместите объекты на один слой."));
        if (source->isSelectionLocked())
            return fail(tr("Заблокированные объекты нельзя объединять."));
        if (source->isExportEnabled() != exportEnabled)
            return fail(tr("Для объединения у объектов должен совпадать флаг включения в экспорт."));
        if (source->isViewVisible() != m_objects[selected.first()]->isViewVisible())
            return fail(tr("Для объединения у объектов должна совпадать видимость. Включите или выключите глазик у выбранных объектов."));
        resizeLocked |= !source->canResize();
        QList<GroupState> states;
        QList<QImage> masks;
        if (const auto *image = dynamic_cast<const ImageObject *>(source.data())) {
            if (image->isRotationObject())
                return fail(tr("Объединять можно только статические изображения и staticgroup. Вращающиеся объекты и текст не поддерживаются."));
            const auto components = image->maskComponents();
            if (components.isEmpty()) {
                states.append({qRound(image->x), qRound(image->y), qMax(1,qRound(image->width)), qMax(1,qRound(image->height)), 0, image->effectiveMaskColor(), true});
                masks.append(image->renderedImage());
            } else {
                for (const auto &component : components) {
                    states.append({qRound(image->x + component.bounds.x()), qRound(image->y + component.bounds.y()),
                        component.bounds.width(), component.bounds.height(), 0, component.color, true});
                    masks.append(component.mask);
                }
            }
        } else if (const auto *group = dynamic_cast<const StaticGroupObject *>(source.data()); group && !dynamic_cast<const TextObject *>(group)) {
            states = group->states;
            masks = group->maskImages;
        } else {
            return fail(tr("Объединять можно только статические изображения и staticgroup. Вращающиеся объекты и текст не поддерживаются."));
        }
        if (states.isEmpty() || masks.size() != states.size())
            return fail(tr("В статическом объекте отсутствуют растровые маски."));
        if (merged->states.size() + states.size() > FpgaResourceLimits::staticParameters())
            return fail(tr("Объединённой группе требуется %1 init; максимум — %2. Учитываются все цветовые слои изображений.")
                .arg(merged->states.size() + states.size()).arg(FpgaResourceLimits::staticParameters()));
        for (int i = 0; i < states.size(); ++i) {
            auto state = states[i];
            const qint64 area = qint64(state.w) * state.h;
            if (state.w <= 0 || state.h <= 0 || state.w > 4095 || state.h > 4095 || masks[i].isNull())
                return fail(tr("Некорректные габариты или маска статического объекта."));
            if (qint64(nextAddress) + area > FpgaResourceLimits::staticPixels())
                return fail(tr("Растрам объединённой группы требуется более %1 ячеек. Уменьшите изображения или объедините меньше объектов.").arg(FpgaResourceLimits::staticPixels()));
            state.addr = nextAddress;
            nextAddress += int(area);
            merged->states.append(state);
            merged->maskImages.append(masks[i].size() == QSize(state.w, state.h)
                ? masks[i] : masks[i].scaled(state.w, state.h, Qt::IgnoreAspectRatio, Qt::FastTransformation));
        }
    }
    // Нельзя незаметно поднять нижний слой над пересекающимся промежуточным объектом.
    for (int index = selected.first(); index <= selected.last(); ++index) {
        if (selected.contains(index)) continue;
        for (int member : selected) {
            if (member > index && m_objects[index]->getBoundingRect().intersects(m_objects[member]->getBoundingRect()))
                return fail(tr("Между выбранными слоями есть пересекающийся объект. Сначала расположите объединяемые слои рядом в списке, чтобы сохранить порядок наложения."));
        }
    }
    merged->groupNumber = merged->states.size();
    merged->setCustomName(tr("Статическая группа"));
    merged->setExportEnabled(exportEnabled);
    merged->setViewVisible(m_objects[selected.first()]->isViewVisible());
    merged->setResizeLocked(resizeLocked);
    merged->setImportedHardwareObject(resizeLocked);
    const auto report = FpgaResourceLimits::inspect(*merged);
    if (!report.errors.isEmpty())
        return fail(report.errors.join('\n'));

    recordHistory();
    QList<QSharedPointer<BaseObject>> objects;
    QStringList tags;
    QMap<int,int> oldToNew;
    int mergedIndex = -1;
    for (int i = 0; i < m_objects.size(); ++i) {
        if (i == selected.first()) {
            mergedIndex = objects.size(); objects.append(merged); tags.append("staticgroup");
        }
        if (selected.contains(i)) continue;
        oldToNew.insert(i, objects.size()); objects.append(m_objects[i]); tags.append(m_objectTags.value(i));
    }
    m_objects = objects; m_objectTags = tags;
    for (auto &group : m_groups) {
        QList<int> members;
        for (int old : group.members) if (oldToNew.contains(old)) members.append(oldToNew[old]);
        group.members = members;
    }
    m_groups.erase(std::remove_if(m_groups.begin(),m_groups.end(),[](const ObjectGroup &group){ return group.members.size()<2; }),m_groups.end());
    emit projectChanged();
    emit logMessage(tr("Объединено статических объектов: %1; init: %2/%4, память: %3/%5.")
        .arg(selected.size()).arg(merged->states.size()).arg(nextAddress).arg(FpgaResourceLimits::staticParameters()).arg(FpgaResourceLimits::staticPixels()));
    return mergedIndex;
}

int ProjectManager::groupObjects(const QList<int> &indexes)
{
    QList<int> normalized;
    QSet<int> seen;
    for (int index : indexes) {
        if (index < 0 || index >= m_objects.size() || seen.contains(index))
            continue;
        seen.insert(index);
        normalized.append(index);
    }
    std::sort(normalized.begin(), normalized.end());
    if (normalized.size() < 2)
        return -1;

    recordHistory();
    for (ObjectGroup &group : m_groups) {
        QList<int> kept;
        for (int member : group.members) {
            if (!seen.contains(member))
                kept.append(member);
        }
        group.members = kept;
    }
    m_groups.erase(std::remove_if(m_groups.begin(), m_groups.end(), [](const ObjectGroup &group) {
        return group.members.size() < 2;
    }), m_groups.end());
    syncNextGroupId();

    ObjectGroup group;
    group.id = m_nextGroupId++;
    group.name = tr("Группа %1").arg(group.id);
    group.members = normalized;
    m_groups.append(group);

    emit projectChanged();
    emit logMessage(tr("Создана группа: %1 объектов").arg(group.members.size()));
    return group.id;
}

bool ProjectManager::ungroupObjects(const QList<int> &indexes)
{
    QSet<int> selected;
    for (int index : indexes) {
        if (index >= 0 && index < m_objects.size())
            selected.insert(index);
    }
    if (selected.isEmpty())
        return false;

    QList<int> removeGroupIds;
    for (const ObjectGroup &group : m_groups) {
        for (int member : group.members) {
            if (selected.contains(member)) {
                removeGroupIds.append(group.id);
                break;
            }
        }
    }
    if (removeGroupIds.isEmpty())
        return false;

    recordHistory();
    const QSet<int> removeSet(removeGroupIds.begin(), removeGroupIds.end());
    m_groups.erase(std::remove_if(m_groups.begin(), m_groups.end(), [&removeSet](const ObjectGroup &group) {
        return removeSet.contains(group.id);
    }), m_groups.end());

    emit projectChanged();
    emit logMessage(tr("Группа разгруппирована"));
    return true;
}

bool ProjectManager::addObjectToGroup(int groupId, int index, bool recordChange)
{
    if (index < 0 || index >= m_objects.size())
        return false;

    for (int i = 0; i < m_groups.size(); ++i) {
        if (m_groups.at(i).id != groupId)
            continue;
        if (m_groups.at(i).members.contains(index))
            return true;

        if (recordChange)
            recordHistory();
        ObjectGroup &group = m_groups[i];
        group.members.append(index);
        std::sort(group.members.begin(), group.members.end());
        emit projectChanged();
        return true;
    }

    return false;
}

QList<int> ProjectManager::groupMembersForObject(int index) const
{
    for (const ObjectGroup &group : m_groups) {
        if (group.members.contains(index))
            return group.members;
    }
    return {};
}

QList<int> ProjectManager::groupMembers(int groupId) const
{
    for (const ObjectGroup &group : m_groups) {
        if (group.id == groupId)
            return group.members;
    }
    return {};
}

QString ProjectManager::groupName(int groupId) const
{
    for (const ObjectGroup &group : m_groups) {
        if (group.id == groupId)
            return group.name;
    }
    return {};
}

bool ProjectManager::renameGroup(int groupId, const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return false;

    for (int i = 0; i < m_groups.size(); ++i) {
        if (m_groups.at(i).id != groupId)
            continue;
        if (m_groups.at(i).name == trimmed)
            return true;
        recordHistory();
        m_groups[i].name = trimmed;
        emit projectChanged();
        return true;
    }
    return false;
}

bool ProjectManager::setGroupViewVisible(int groupId, bool visible)
{
    QList<int> members;
    for (const ObjectGroup &group : m_groups) {
        if (group.id == groupId) {
            members = group.members;
            break;
        }
    }
    if (members.isEmpty())
        return false;

    bool hasChanges = false;
    for (int index : members) {
        if (index >= 0 && index < m_objects.size() && m_objects[index]->isViewVisible() != visible) {
            hasChanges = true;
            break;
        }
    }
    if (!hasChanges)
        return true;

    recordHistory();
    for (int index : members) {
        if (index >= 0 && index < m_objects.size())
            m_objects[index]->setViewVisible(visible);
    }
    emit projectChanged();
    return true;
}

bool ProjectManager::setGroupExportEnabled(int groupId, bool enabled)
{
    QList<int> members;
    for (const ObjectGroup &group : m_groups) {
        if (group.id == groupId) {
            members = group.members;
            break;
        }
    }
    if (members.isEmpty())
        return false;

    bool hasChanges = false;
    for (int index : members) {
        if (index >= 0 && index < m_objects.size() && !isSymmetryCopy(index) && m_objects[index]->isExportEnabled() != enabled) {
            hasChanges = true;
            break;
        }
    }
    if (!hasChanges)
        return true;

    recordHistory();
    for (int index : members) {
        if (index >= 0 && index < m_objects.size())
            m_objects[index]->setExportEnabled(enabled);
    }
    emit projectChanged();
    return true;
}

bool ProjectManager::renameObject(int index, const QString &name)
{
    if (index < 0 || index >= m_objects.size())
        return false;

    const QString trimmed = name.trimmed();
    if (m_objects[index]->customName() == trimmed)
        return true;

    recordHistory();
    m_objects[index]->setCustomName(trimmed);
    emit projectChanged();
    return true;
}

bool ProjectManager::copyObjects(const QList<int> &indexes)
{
    for (int index : indexes) if (isSymmetryCopy(index)) return false;
    m_clipboardObjects.clear();
    m_clipboardTags.clear();
    m_clipboardGroups.clear();

    QSet<int> seen;
    QMap<int, int> sourceToClipboard;
    for (int index : indexes) {
        if (index < 0 || index >= m_objects.size() || seen.contains(index))
            continue;
        seen.insert(index);
        sourceToClipboard.insert(index, m_clipboardObjects.size());
        m_clipboardObjects.append(m_objects[index]->clone());
        m_clipboardTags.append(index < m_objectTags.size() ? m_objectTags[index] : QString());
    }

    if (m_clipboardObjects.isEmpty())
        return false;

    for (const ObjectGroup &group : m_groups) {
        bool groupFullyCopied = !group.members.isEmpty();
        ObjectGroup copiedGroup;
        copiedGroup.id = group.id;
        copiedGroup.name = group.name;
        for (int member : group.members) {
            if (!sourceToClipboard.contains(member)) {
                groupFullyCopied = false;
                break;
            }
            copiedGroup.members.append(sourceToClipboard.value(member));
        }
        if (groupFullyCopied && copiedGroup.members.size() >= 2) {
            std::sort(copiedGroup.members.begin(), copiedGroup.members.end());
            m_clipboardGroups.append(copiedGroup);
        }
    }

    emit logMessage(tr("Скопировано объектов: %1").arg(m_clipboardObjects.size()));
    return true;
}

void ProjectManager::syncNextGroupId()
{
    int nextId = 1;
    for (const ObjectGroup &group : m_groups)
        nextId = qMax(nextId, group.id + 1);
    m_nextGroupId = qMax(m_nextGroupId, nextId);
}

QList<int> ProjectManager::pasteObjects(std::optional<QPointF> position)
{
    QList<int> pastedIndexes;
    if (m_clipboardObjects.isEmpty())
        return pastedIndexes;

    QList<QSharedPointer<BaseObject>> clones;
    QRectF bounds;
    for (const auto &source : m_clipboardObjects) {
        const auto clone = source->clone();
        if (!clone) return {};
        bounds = clones.isEmpty() ? clone->getBoundingRect() : bounds.united(clone->getBoundingRect());
        clone->setEditorLayerId(m_activeLayerId);
        clones.append(clone);
    }
    if (position && (!qIsFinite(position->x()) || !qIsFinite(position->y()))) return {};
    // Центр общего габарита оказывается под курсором. Коррекция у границ
    // применяется ко всему набору, чтобы расстояния внутри группы не менялись.
    QPointF delta = position ? *position - bounds.center() : QPointF(24.0, 24.0);
    const QRectF moved = bounds.translated(delta);
    const double canvasW = m_document.canvasWidth(), canvasH = m_document.canvasHeight();
    delta.rx() += moved.width() > canvasW ? -moved.left()
        : qBound(-moved.left(), 0.0, canvasW - moved.right());
    delta.ry() += moved.height() > canvasH ? -moved.top()
        : qBound(-moved.top(), 0.0, canvasH - moved.bottom());
    for (const auto &clone : clones) clone->moveBy(delta.x(), delta.y());

    recordHistory();
    const int pasteCount = m_clipboardObjects.size();
    for (ObjectGroup &group : m_groups) {
        for (int &member : group.members)
            member += pasteCount;
    }

    for (int i = m_clipboardObjects.size() - 1; i >= 0; --i) {
        m_objects.prepend(clones[i]);
        m_objectTags.prepend(i < m_clipboardTags.size() ? m_clipboardTags[i] : QString());
    }

    for (int i = 0; i < m_clipboardObjects.size(); ++i)
        pastedIndexes.append(i);

    syncNextGroupId();
    for (const ObjectGroup &clipboardGroup : m_clipboardGroups) {
        ObjectGroup group;
        group.id = m_nextGroupId++;
        group.name = clipboardGroup.name;
        if (group.name.isEmpty() || group.name.startsWith(tr("Группа ")))
            group.name = tr("Группа %1").arg(group.id);
        group.members = clipboardGroup.members;
        group.members.erase(std::remove_if(group.members.begin(), group.members.end(), [pasteCount](int member) {
            return member < 0 || member >= pasteCount;
        }), group.members.end());
        std::sort(group.members.begin(), group.members.end());
        group.members.erase(std::unique(group.members.begin(), group.members.end()), group.members.end());
        if (group.members.size() >= 2)
            m_groups.append(group);
    }

    emit projectChanged();
    emit logMessage(tr("Вставлено объектов: %1").arg(pastedIndexes.size()));
    return pastedIndexes;
}

bool ProjectManager::canPasteObjects() const
{
    return !m_clipboardObjects.isEmpty();
}

bool ProjectManager::isSymmetryCopy(int index) const
{
    return dynamic_cast<SymmetryCopyObject *>(getObjectAt(index).data()) != nullptr;
}

QList<QSharedPointer<SymmetryAxis>> ProjectManager::symmetryAxes() const
{
    QList<QSharedPointer<SymmetryAxis>> axes;
    for (const auto &object : m_objects) {
        const auto *copy = dynamic_cast<SymmetryCopyObject *>(object.data());
        if (copy && !axes.contains(copy->axis())) axes.append(copy->axis());
    }
    return axes;
}

bool ProjectManager::canCreateSymmetry(const QList<int> &indexes) const
{
    if (indexes.isEmpty() || !m_document.hasCanvas()) return false;
    for (int index : indexes) {
        const auto object = getObjectAt(index);
        if (!supportsObjectReflection(object.data()) || object->isSelectionLocked()
            || !isObjectVisibleInEditor(object.data())) return false;
    }
    return true;
}

QList<int> ProjectManager::createSymmetry(const QList<int> &indexes, Qt::Orientation orientation)
{
    m_lastErrorMessage.clear();
    if (!canCreateSymmetry(indexes) || (orientation != Qt::Vertical && orientation != Qt::Horizontal)) return {};
    auto axis = QSharedPointer<SymmetryAxis>::create();
    axis->id = 1;
    for (const auto &existing : symmetryAxes()) axis->id = qMax(axis->id, existing->id + 1);
    axis->orientation = orientation;
    axis->position = (orientation == Qt::Vertical ? getCanvasWidth() : getCanvasHeight()) / 2.0;
    const QSet<int> selected(indexes.begin(), indexes.end());
    QList<QSharedPointer<BaseObject>> objects;
    QStringList tags;
    QList<int> copies;
    QMap<int, int> remap;
    recordHistory();
    for (int i = 0; i < m_objects.size(); ++i) {
        const auto source = m_objects[i];
        remap.insert(i, objects.size());
        objects.append(source); tags.append(m_objectTags.value(i));
        if (!selected.contains(i)) continue;
        auto copy = QSharedPointer<SymmetryCopyObject>::create(source, axis);
        copy->setEditorLayerId(source->editorLayerId());
        copy->setCustomName((source->customName().isEmpty() ? source->getDisplayName() : source->customName()) + tr(" — копия"));
        copies.append(objects.size()); objects.append(copy); tags.append(QStringLiteral("symmetry-copy"));
    }
    for (auto &group : m_groups) for (int &member : group.members) member = remap.value(member);
    m_objects = objects; m_objectTags = tags;
    emit projectChanged();
    emit logMessage(tr("Создана симметрия: %1 копий").arg(copies.size()));
    return copies;
}

bool ProjectManager::detachSymmetryCopy(int index)
{
    const auto *copy = dynamic_cast<SymmetryCopyObject *>(getObjectAt(index).data());
    if (!copy) return false;
    auto object = copy->source()->clone();
    if (!object || !reflectObject(object.data(), *copy->axis())) return false;
    const int sourceIndex = m_objects.indexOf(copy->source());
    if (sourceIndex < 0) return false;
    object->setCustomName(copy->customName());
    object->setEditorLayerId(copy->editorLayerId());
    object->setViewVisible(copy->isViewVisible());
    object->setSelectionLocked(copy->isSelectionLocked());
    object->setExportEnabled(true);
    recordHistory();
    m_objectTags[index] = m_objectTags.value(sourceIndex);
    m_objects[index] = object;
    emit projectChanged();
    emit logMessage(tr("Копия стала независимым объектом"));
    return true;
}

bool ProjectManager::setSymmetryAxisPosition(int id, double position, bool recordChange)
{
    if (!qIsFinite(position)) return false;
    for (const auto &axis : symmetryAxes()) {
        if (axis->id != id) continue;
        const bool vertical = axis->orientation == Qt::Vertical;
        const double size = vertical ? getCanvasWidth() : getCanvasHeight();
        double minimum = 0, maximum = size;
        for (const auto &object : m_objects) {
            const auto *copy = dynamic_cast<SymmetryCopyObject *>(object.data());
            if (!copy || copy->axis() != axis) continue;
            const auto bounds = copy->source()->getBoundingRect();
            minimum = qMax(minimum, (vertical ? bounds.right() : bounds.bottom()) / 2);
            maximum = qMin(maximum, (size + (vertical ? bounds.left() : bounds.top())) / 2);
        }
        if (minimum > maximum) return false;
        position = qBound(minimum, qRound(position * 2) / 2.0, maximum);
        if (qAbs(position - axis->position) < 1e-9) return false;
        if (recordChange) recordHistory();
        axis->position = position;
        for (const auto &object : m_objects) {
            const auto *copy = dynamic_cast<SymmetryCopyObject *>(object.data());
            if (copy && copy->axis() == axis) emit object->changed();
        }
        emit projectChanged();
        return true;
    }
    return false;
}

QPointF ProjectManager::constrainSymmetryMove(const QList<int> &indexes, const QPointF &delta) const
{
    double minX = -1e30, maxX = 1e30, minY = -1e30, maxY = 1e30;
    for (const auto &object : m_objects) {
        const auto *copy = dynamic_cast<SymmetryCopyObject *>(object.data());
        if (!copy || !indexes.contains(m_objects.indexOf(copy->source()))) continue;
        const auto bounds = copy->getBoundingRect();
        if (copy->axis()->orientation == Qt::Vertical) {
            minX = qMax(minX, bounds.right() - getCanvasWidth()); maxX = qMin(maxX, bounds.left());
            minY = qMax(minY, -bounds.top()); maxY = qMin(maxY, getCanvasHeight() - bounds.bottom());
        } else {
            minX = qMax(minX, -bounds.left()); maxX = qMin(maxX, getCanvasWidth() - bounds.right());
            minY = qMax(minY, bounds.bottom() - getCanvasHeight()); maxY = qMin(maxY, bounds.top());
        }
    }
    return {minX <= maxX ? qBound(minX, delta.x(), maxX) : 0.0,
            minY <= maxY ? qBound(minY, delta.y(), maxY) : 0.0};
}

bool ProjectManager::symmetryFitsCanvas(const BaseObject *source, const QRectF &bounds) const
{
    for (const auto &object : m_objects) {
        const auto *copy = dynamic_cast<SymmetryCopyObject *>(object.data());
        if (!copy || copy->source().data() != source) continue;
        const auto reflected = copy->reflection().mapRect(bounds);
        if (reflected.left() < -1e-6 || reflected.top() < -1e-6
            || reflected.right() > getCanvasWidth() + 1e-6 || reflected.bottom() > getCanvasHeight() + 1e-6) return false;
    }
    return true;
}

bool ProjectManager::canEditGeometry(int index, const std::function<void(BaseObject *)> &edit, bool clampToCanvas) const
{
    const auto source = getObjectAt(index);
    if (!source) return false;
    bool linked = false;
    for (const auto &object : m_objects) {
        const auto *copy = dynamic_cast<SymmetryCopyObject *>(object.data());
        if (copy && copy->source() == source) { linked = true; break; }
    }
    if (!linked) return true;
    const auto probe = source->clone();
    if (!probe) return false;
    edit(probe.data());
    if (clampToCanvas) CanvasGeometry::keepInside(probe.data(), getCanvasWidth(), getCanvasHeight());
    return symmetryFitsCanvas(source.data(), probe->getBoundingRect());
}

bool ProjectManager::alignObject(int index, ObjectAlignment alignment)
{
    if (index < 0 || index >= m_objects.size() || !m_document.hasCanvas())
        return false;

    const auto object = m_objects[index];
    if (!object->canMove()) return false;
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
        dx = m_document.canvasWidth() / 2.0 - bounds.center().x();
        break;
    case ObjectAlignment::Right:
        dx = m_document.canvasWidth() - bounds.right();
        break;
    case ObjectAlignment::Top:
        dy = -bounds.top();
        break;
    case ObjectAlignment::VCenter:
        dy = m_document.canvasHeight() / 2.0 - bounds.center().y();
        break;
    case ObjectAlignment::Bottom:
        dy = m_document.canvasHeight() - bounds.bottom();
        break;
    }

    const auto delta = constrainSymmetryMove({index}, QPointF(dx, dy));
    if (qFuzzyIsNull(delta.x()) && qFuzzyIsNull(delta.y())) return true;
    recordHistory();
    object->moveBy(delta.x(), delta.y());
    emit projectChanged();
    emit logMessage(tr("Выполнено выравнивание объекта"));
    return true;
}

bool ProjectManager::setObjectSelectionLocked(int index, bool locked)
{
    const auto object = getObjectAt(index);
    if (!object || object->isSelectionLocked() == locked)
        return false;
    recordHistory();
    object->setSelectionLocked(locked);
    emit projectChanged();
    return true;
}

bool ProjectManager::setGroupSelectionLocked(int groupId, bool locked)
{
    const auto members = groupMembers(groupId);
    bool changed = false;
    for (int member : members) {
        const auto object = getObjectAt(member);
        if (object && object->isSelectionLocked() != locked) {
            if (!changed) recordHistory();
            object->setSelectionLocked(locked);
            changed = true;
        }
    }
    if (changed) emit projectChanged();
    return changed;
}

bool ProjectManager::setObjectViewVisible(int index, bool visible)
{
    if (index < 0 || index >= m_objects.size())
        return false;

    if (m_objects[index]->isViewVisible() == visible)
        return true;

    recordHistory();
    m_objects[index]->setViewVisible(visible);
    emit projectChanged();
    return true;
}

bool ProjectManager::setObjectExportEnabled(int index, bool enabled)
{
    if (index < 0 || index >= m_objects.size())
        return false;

    if (isSymmetryCopy(index)) return !enabled;

    if (m_objects[index]->isExportEnabled() == enabled)
        return true;

    recordHistory();
    m_objects[index]->setExportEnabled(enabled);
    emit projectChanged();
    return true;
}

void ProjectManager::recordObjectEdit()
{
    recordHistory();
}

void ProjectManager::finishObjectEdit(const QString &message)
{
    emit projectChanged();
    if (!message.isEmpty())
        emit logMessage(message);
}

bool ProjectManager::setObjectProperty(BaseObject *object, const QString &name, const QString &value)
{
    for (const auto &candidate : m_objects)
        if (candidate.data() == object) return setObjectsProperty({candidate}, name, value);
    return false;
}

bool ProjectManager::isObjectPropertyEditable(const BaseObject *object, const QString &name) const
{
    for (const auto &candidate : m_objects) {
        if (candidate.data() != object) continue;
        const auto probe = candidate->clone();
        if (!probe) return false;
        for (const auto &property : candidate->getProperties())
            if (property.first == name) return probe->setObjectProperty(name, property.second);
    }
    return false;
}

bool ProjectManager::setObjectRotationPivot(int index, const QPointF &canvasPosition)
{
    const auto object = getObjectAt(index);
    if (!object || !object->supportsRotationHandle() || object->isSelectionLocked()
        || !isObjectVisibleInEditor(object.data()) || !qIsFinite(canvasPosition.x()) || !qIsFinite(canvasPosition.y())
        || !object->contains(canvasPosition)) return false;
    bool invertible = false;
    const auto inverse = object->rotationFrame().inverted(&invertible);
    if (!invertible) return false;
    const auto local = inverse.map(canvasPosition);
    if (!qIsFinite(local.x()) || !qIsFinite(local.y())) return false;
    if (object->editorRotationPivot() && QLineF(*object->editorRotationPivot(), local).length() < 1e-9) return false;
    recordHistory();
    object->setEditorRotationPivot(local);
    finishObjectEdit(tr("Изменён редакторский центр вращения"));
    return true;
}

bool ProjectManager::setObjectsProperty(const QList<QSharedPointer<BaseObject>> &objects,
                                       const QString &name, const QString &value)
{
    m_lastErrorMessage.clear();
    QString resolvedValue = value;
    if (!objects.isEmpty() && std::all_of(objects.cbegin(), objects.cend(), [&name](const auto &object) {
        return object && object->isNumericProperty(name);
    })) {
        double result = 0.0;
        if (!NumericExpression::evaluate(value, &result)) {
            m_lastErrorMessage = tr("Некорректное выражение. Используйте числа, +, −, *, / и скобки. Деление на ноль недопустимо.");
            return false;
        }
        resolvedValue = QString::number(result, 'g', std::numeric_limits<double>::digits10);
    }
    QList<QSharedPointer<BaseObject>> changed;
    for (const auto &object : objects) {
        if (!object || !m_objects.contains(object)) return false;
        const auto properties = object->getProperties();
        const auto it = std::find_if(properties.cbegin(), properties.cend(), [&name](const auto &p) { return p.first == name; });
        if (it == properties.cend()) return false;
        if (it->second == resolvedValue || changed.contains(object)) continue;
        const auto probe = object->clone();
        if (!probe || !probe->setObjectProperty(name, resolvedValue)) {
            m_lastErrorMessage = probe ? probe->lastValidationMessage() : QString();
            if (m_lastErrorMessage.isEmpty()) m_lastErrorMessage = tr("Значение параметра «%1» недопустимо для одного из выбранных объектов.").arg(name);
            return false;
        }
        const QRectF bounds = probe->getBoundingRect();
        if (!qIsFinite(bounds.x()) || !qIsFinite(bounds.y()) || !qIsFinite(bounds.width()) || !qIsFinite(bounds.height())
            || bounds.width() < 0 || bounds.height() < 0) {
            m_lastErrorMessage = tr("Координаты должны быть конечными числами, а размеры не могут быть отрицательными.");
            return false;
        }
        CanvasGeometry::keepInside(probe.data(), getCanvasWidth(), getCanvasHeight());
        if (!symmetryFitsCanvas(object.data(), probe->getBoundingRect())) {
            m_lastErrorMessage = tr("Изменение выводит симметричную копию за границы холста. Переместите ось или уменьшите объект.");
            return false;
        }
        changed.append(object);
    }
    if (changed.isEmpty()) return !objects.isEmpty();
    recordHistory();
    for (const auto &object : changed) {
        object->setObjectProperty(name, resolvedValue);
        CanvasGeometry::keepInside(object.data(), m_document.canvasWidth(), m_document.canvasHeight());
    }
    emit projectChanged();
    return true;
}

void ProjectManager::resetEditorLayers()
{
    m_editorLayers = {{1, tr("Слой 1"), QColor(QStringLiteral("#00c8f0"))}};
    m_nextLayerId = 2;
    m_activeLayerId = 1;
    m_showAllLayers = false;
}

bool ProjectManager::hasEditorLayer(int id) const
{
    return std::any_of(m_editorLayers.cbegin(), m_editorLayers.cend(), [id](const auto &layer) { return layer.id == id; });
}

QString ProjectManager::editorLayerName(int id) const
{
    for (const auto &layer : m_editorLayers) if (layer.id == id) return layer.name;
    return {};
}

QColor ProjectManager::editorLayerColor(int id) const
{
    for (const auto &layer : m_editorLayers) if (layer.id == id) return layer.selectionColor;
    return QColor(QStringLiteral("#00c8f0"));
}

int ProjectManager::layerObjectCount(int id) const
{
    return std::count_if(m_objects.cbegin(), m_objects.cend(), [id](const auto &o) { return o->editorLayerId() == id; });
}

bool ProjectManager::isObjectOnVisibleLayer(const BaseObject *object) const
{
    return object && (m_showAllLayers || object->editorLayerId() == m_activeLayerId);
}

bool ProjectManager::isObjectVisibleInEditor(const BaseObject *object) const
{
    return isObjectOnVisibleLayer(object) && object->isViewVisible();
}

void ProjectManager::setActiveLayer(int id)
{
    if (!hasEditorLayer(id) || id == m_activeLayerId) return;
    m_activeLayerId = id;
    emit editorLayerViewChanged();
}

void ProjectManager::setShowAllLayers(bool enabled)
{
    if (enabled == m_showAllLayers) return;
    m_showAllLayers = enabled;
    emit editorLayerViewChanged();
}

void ProjectManager::notifyEditorLayersChanged()
{
    emit editorLayersChanged();
    emit editorLayerViewChanged();
    emit projectChanged();
}

QString ProjectManager::uniqueLayerName(const QString &base) const
{
    QString name = base;
    int suffix = 2;
    auto exists = [this](const QString &candidate) {
        return std::any_of(m_editorLayers.cbegin(), m_editorLayers.cend(), [&candidate](const auto &l) { return l.name.compare(candidate, Qt::CaseInsensitive) == 0; });
    };
    while (exists(name)) name = base + QStringLiteral(" (%1)").arg(suffix++);
    return name;
}

int ProjectManager::addEditorLayer()
{
    recordHistory();
    const int id = m_nextLayerId++;
    static const QStringList colors = {"#00c8f0", "#ff5c7a", "#ffca58", "#8bdff2"};
    m_editorLayers.append({id, uniqueLayerName(tr("Слой %1").arg(id)), QColor(colors[(id - 1) % colors.size()])});
    m_activeLayerId = id;
    notifyEditorLayersChanged();
    return id;
}

bool ProjectManager::renameEditorLayer(int id, const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty() || !hasEditorLayer(id)) return false;
    for (const auto &layer : m_editorLayers)
        if (layer.id != id && layer.name.compare(trimmed, Qt::CaseInsensitive) == 0) return false;
    for (int i = 0; i < m_editorLayers.size(); ++i) {
        if (m_editorLayers.at(i).id != id) continue;
        if (m_editorLayers.at(i).name == trimmed) return true;
        recordHistory();
        m_editorLayers[i].name = trimmed;
        notifyEditorLayersChanged();
        return true;
    }
    return false;
}

bool ProjectManager::setEditorLayerColor(int id, const QColor &color)
{
    if (!color.isValid()) return false;
    for (int i = 0; i < m_editorLayers.size(); ++i) {
        if (m_editorLayers.at(i).id != id) continue;
        if (m_editorLayers.at(i).selectionColor == color) return true;
        recordHistory();
        m_editorLayers[i].selectionColor = color;
        notifyEditorLayersChanged();
        return true;
    }
    return false;
}

bool ProjectManager::setObjectsLayer(const QList<QSharedPointer<BaseObject>> &objects, int layerId)
{
    if (!hasEditorLayer(layerId) || objects.isEmpty()) return false;
    for (const auto &object : objects) if (dynamic_cast<SymmetryCopyObject *>(object.data())) return false;
    for (const auto &object : objects) if (!object || !m_objects.contains(object)) return false;
    if (std::all_of(objects.cbegin(), objects.cend(), [layerId](const auto &o) { return o->editorLayerId() == layerId; })) return true;
    recordHistory();
    for (const auto &object : objects) {
        object->setEditorLayerId(layerId);
        for (const auto &candidate : m_objects) {
            const auto *copy = dynamic_cast<SymmetryCopyObject *>(candidate.data());
            if (copy && copy->source() == object) candidate->setEditorLayerId(layerId);
        }
    }
    notifyEditorLayersChanged();
    return true;
}

int ProjectManager::duplicateEditorLayer(int id)
{
    m_lastErrorMessage.clear();
    if (!hasEditorLayer(id)) return -1;
    QList<QSharedPointer<BaseObject>> copies;
    QStringList tags;
    QMap<int, int> copiedIndexes;
    for (int i = 0; i < m_objects.size(); ++i) {
        if (m_objects[i]->editorLayerId() != id) continue;
        if (dynamic_cast<ThreeDViewObject *>(m_objects[i].data())) {
            m_lastErrorMessage = tr("Нельзя дублировать слой с 3D-видом: в проекте допустим только один 3D-вид.");
            return -1;
        }
        copiedIndexes.insert(i, copies.size());
        copies.append(m_objects[i]);
        tags.append(m_objectTags.value(i));
    }
    copies = ObjectCloner::cloneScene(copies);
    QMap<const SymmetryAxis *, int> axisIds;
    int nextAxisId = 1;
    for (const auto &axis : symmetryAxes()) nextAxisId = qMax(nextAxisId, axis->id + 1);
    for (const auto &object : copies) {
        const auto *copy = dynamic_cast<SymmetryCopyObject *>(object.data());
        if (!copy) continue;
        if (!axisIds.contains(copy->axis().data())) axisIds.insert(copy->axis().data(), nextAxisId++);
        copy->axis()->id = axisIds.value(copy->axis().data());
    }
    recordHistory();
    const int newId = m_nextLayerId++;
    m_editorLayers.append({newId, uniqueLayerName(editorLayerName(id) + tr(" — копия")), editorLayerColor(id)});
    auto groups = m_groups;
    for (auto &group : m_groups) for (int &index : group.members) index += copies.size();
    for (const auto &group : groups) {
        ObjectGroup copy{m_nextGroupId, group.name, {}};
        for (int index : group.members) if (copiedIndexes.contains(index)) copy.members.append(copiedIndexes.value(index));
        if (copy.members.size() >= 2) { ++m_nextGroupId; m_groups.append(copy); }
    }
    for (const auto &copy : copies) copy->setEditorLayerId(newId);
    m_objects = copies + m_objects;
    m_objectTags = tags + m_objectTags;
    m_activeLayerId = newId;
    emit editorLayerObjectsChanged();
    notifyEditorLayersChanged();
    return newId;
}

bool ProjectManager::mergeEditorLayer(int sourceId, int targetId)
{
    return mergeEditorLayers({sourceId, targetId}, targetId);
}

bool ProjectManager::mergeEditorLayers(const QList<int> &layerIds, int targetId)
{
    const QSet<int> selected(layerIds.cbegin(), layerIds.cend());
    if (selected.size() < 2 || !selected.contains(targetId)) return false;
    for (int id : selected) if (!hasEditorLayer(id)) return false;
    recordHistory();
    for (const auto &object : m_objects)
        if (selected.contains(object->editorLayerId())) object->setEditorLayerId(targetId);
    m_editorLayers.erase(std::remove_if(m_editorLayers.begin(), m_editorLayers.end(), [&selected, targetId](const auto &layer) {
        return layer.id != targetId && selected.contains(layer.id);
    }), m_editorLayers.end());
    m_activeLayerId = targetId;
    notifyEditorLayersChanged();
    return true;
}

bool ProjectManager::removeEditorLayer(int id)
{
    if (m_editorLayers.size() <= 1 || !hasEditorLayer(id)) return false;
    recordHistory();
    QMap<int, int> remaining;
    for (int i = m_objects.size() - 1; i >= 0; --i) {
        if (m_objects[i]->editorLayerId() == id) { m_objects.removeAt(i); m_objectTags.removeAt(i); }
    }
    // Сопоставление старых индексов по сохраненным объектам снимка.
    const auto &before = m_undoStack.last().objects;
    int next = 0;
    for (int i = 0; i < before.size(); ++i) if (before[i]->editorLayerId() != id) remaining.insert(i, next++);
    for (auto &group : m_groups) {
        QList<int> members;
        for (int index : group.members) if (remaining.contains(index)) members.append(remaining.value(index));
        group.members = members;
    }
    m_groups.erase(std::remove_if(m_groups.begin(), m_groups.end(), [](const auto &g) { return g.members.size() < 2; }), m_groups.end());
    int removedPosition = 0;
    for (int i = 0; i < m_editorLayers.size(); ++i) if (m_editorLayers[i].id == id) removedPosition = i;
    m_editorLayers.removeAt(removedPosition);
    if (m_activeLayerId == id) m_activeLayerId = m_editorLayers[qMin(removedPosition, int(m_editorLayers.size()) - 1)].id;
    emit editorLayerObjectsChanged();
    notifyEditorLayersChanged();
    return true;
}

QDomDocument ProjectManager::buildCompiledFpgaXml() const
{
    return FpgaXmlExporter::compile(m_document, m_objects, m_objectTags);
}

bool ProjectManager::exportFpgaConfiguration(const QString &path, const FpgaHardwareScene &scene, QString *error)
{
    const auto configuration = FpgaPipelineConfig::fromScene(scene, error);
    if (!configuration.isValid() || !configuration.writeFile(path, error)) return false;
    m_fpgaConfiguration = configuration;
    emit logMessage(tr("Конфигурация ПЛИС сохранена: %1; этапов: %2, параметров: %3/%4. Пересборка и программирование платы выполняются отдельно.")
        .arg(path).arg(configuration.stages.size()).arg(configuration.parameterCount()).arg(FpgaPipelineConfig::parameterArraySize));
    return true;
}

bool ProjectManager::saveToFile(const QString &targetFile)
{
    m_lastErrorMessage.clear();
    for (const auto &object : m_objects) {
        if (!object->isExportEnabled()) continue;
        const auto report = FpgaResourceLimits::inspect(*object);
        if (!report.errors.isEmpty()) {
            m_lastErrorMessage = report.errors.join('\n');
            return false;
        }
    }
    const QString outPath = targetFile.isEmpty() ? m_filePath : targetFile;
    if (outPath.isEmpty()) {
        m_lastErrorMessage = tr("Не задан путь для сохранения XML");
        return false;
    }

    QDomDocument doc = buildCompiledFpgaXml();
    if (!FpgaCompiledDocument::fromDomDocument(doc, &m_lastErrorMessage).isValid()) return false;
    EditorProjectXml::State state;
    state.document = m_document;
    state.objects = m_objects; state.objectTags = m_objectTags;
    state.layers = m_editorLayers; state.groups = m_groups;
    state.nextLayerId = m_nextLayerId; state.nextGroupId = m_nextGroupId;
    state.activeLayerId = m_activeLayerId; state.showAllLayers = m_showAllLayers;
    state.showGrid = m_showGrid; state.snapToCanvas = m_snapToCanvas; state.snapToGrid = m_snapToGrid; state.snapToObjects = m_snapToObjects;
    state.gridStep = m_gridStep; state.gridColor = m_gridColor;
    state.canvasGuideColor = m_snapCanvasGuideColor; state.gridGuideColor = m_snapGridGuideColor; state.objectGuideColor = m_snapObjectGuideColor;
    state.view = m_editorViewState;
    state.fpgaConfiguration = m_fpgaConfiguration;
    if (!EditorProjectXml::write(doc, state, &m_lastErrorMessage)) return false;

    if (!ProjectXmlFile::write(outPath, doc, &m_lastErrorMessage)) return false;

    emit logMessage(tr("Проект сохранён: %1").arg(outPath));
    m_lastErrorMessage.clear();
    m_filePath = outPath;
    return true;
}

int ProjectManager::importImage(const QString &fileName, bool rotating)
{
    if (!m_document.hasCanvas()) {
        emit logMessage(tr("Сначала создайте или откройте проект"));
        return -1;
    }

    QImage image;
    const QString suffix = QFileInfo(fileName).suffix().toLower();
    auto *imageObject = new ImageObject(rotating ? ImageObject::Kind::Rotating : ImageObject::Kind::Static);
    if (suffix == QStringLiteral("svg")) {
        QSvgRenderer renderer(fileName);
        QFile svgFile(fileName);
        if (renderer.isValid() && svgFile.open(QIODevice::ReadOnly)) {
            const QSize defaultSize = renderer.defaultSize().isValid() ? renderer.defaultSize() : QSize(128, 128);
            imageObject->setSvgData(svgFile.readAll(), QFileInfo(fileName).fileName(), defaultSize);
        } else {
            delete imageObject;
            emit logMessage(tr("Не удалось импортировать SVG: %1").arg(fileName));
            return -1;
        }
    } else {
        image = QImageReader(fileName).read();
        if (image.isNull()) {
            delete imageObject;
            emit logMessage(tr("Не удалось импортировать изображение: %1").arg(fileName));
            return -1;
        }
        imageObject->setRasterImage(image, QFileInfo(fileName).fileName());
    }

    const int maxW = qMax(1, m_document.canvasWidth() / 3);
    const int maxH = qMax(1, m_document.canvasHeight() / 3);
    if (imageObject->width > maxW || imageObject->height > maxH) {
        const QSizeF fitted = QSizeF(imageObject->width, imageObject->height).scaled(QSizeF(maxW, maxH), Qt::KeepAspectRatio);
        imageObject->width = fitted.width();
        imageObject->height = fitted.height();
    }

    const ParamSchema staticSchema = m_schemas.value(QStringLiteral("staticgroup"),
        FpgaSchemaRegistry::instance()->buildSchema(QStringLiteral("staticgroup")));
    const QSizeF hardwareFitted = ImageResourceBuilder::fitSizeToStaticGroupLimits(QSizeF(imageObject->width, imageObject->height), staticSchema);
    imageObject->width = hardwareFitted.width();
    imageObject->height = hardwareFitted.height();

    imageObject->x = qMax(0.0, (m_document.canvasWidth() - imageObject->width) / 2.0);
    imageObject->y = qMax(0.0, (m_document.canvasHeight() - imageObject->height) / 2.0);

    recordHistory();
    insertObjectAtFront(imageObject, QStringLiteral("image"));
    emit projectChanged();
    emit logMessage(tr("Изображение добавлено: %1").arg(fileName));
    return 0;
}

QSharedPointer<BaseObject> ProjectManager::getObjectAt(int index) const
{
    if (index >= 0 && index < m_objects.size()) {
        return m_objects[index];
    }
    return nullptr;
}

// геттеры
int ProjectManager::getObjectCount() const { return m_objects.size(); }
QString ProjectManager::getProjectName() const { return m_document.projectName(); }
int ProjectManager::getCanvasWidth() const { return m_document.canvasWidth(); }
int ProjectManager::getCanvasHeight() const { return m_document.canvasHeight(); }
QColor ProjectManager::getBackgroundColor() const { return m_document.backgroundColor(); }
QString ProjectManager::getFilePath() const { return m_filePath; }
const QList<QSharedPointer<BaseObject>>& ProjectManager::getObjects() const { return m_objects; }
QList<ProjectManager::ObjectGroup> ProjectManager::objectGroups() const { return m_groups; }
bool ProjectManager::hasThreeDViewObject() const
{
    return threeDViewObject() != nullptr;
}

ThreeDViewObject *ProjectManager::threeDViewObject() const
{
    for (const auto &object : m_objects) {
        if (auto threeDView = dynamic_cast<ThreeDViewObject*>(object.data()))
            return threeDView;
    }
    return nullptr;
}

bool ProjectManager::setAircraftState(double x, double y, double altitude, double course, int screenY, bool recordChange)
{
    auto *object = threeDViewObject();
    if (!object) return false;
    const QString previous = object->serializeInit();
    if (!object->setAircraftState(x, y, altitude, course, screenY)) return false;
    const QString next = object->serializeInit();
    if (recordChange) {
        object->parse(previous, FpgaSchemaRegistry::instance()->buildSchema("layer3d"));
        recordHistory();
        object->parse(next, FpgaSchemaRegistry::instance()->buildSchema("layer3d"));
    }
    const QScopedValueRollback<bool> changingState(m_aircraftStateChange, true);
    emit projectChanged();
    return true;
}
bool ProjectManager::showGrid() const { return m_showGrid; }
bool ProjectManager::snapToCanvas() const { return m_snapToCanvas; }
bool ProjectManager::snapToGrid() const { return m_snapToGrid; }
bool ProjectManager::snapToObjects() const { return m_snapToObjects; }

int ProjectManager::gridStep() const { return m_gridStep; }
QColor ProjectManager::gridColor() const { return m_gridColor; }
QColor ProjectManager::snapCanvasGuideColor() const { return m_snapCanvasGuideColor; }
QColor ProjectManager::snapGridGuideColor() const { return m_snapGridGuideColor; }
QColor ProjectManager::snapObjectGuideColor() const { return m_snapObjectGuideColor; }

void ProjectManager::reloadGlobalSettings()
{
    readGlobalWorkspaceSettings();
    emit projectChanged();
}

void ProjectManager::readGlobalWorkspaceSettings()
{
    auto settings = ApplicationSettings::create();
    m_gridStep = qBound(1, settings.value("gridStep", 10).toInt(), 1000);

    QString colorStr = settings.value("gridColor", "#3778b4c8").toString();
    m_gridColor = QColor(colorStr);
    m_snapCanvasGuideColor = QColor(settings.value("snapCanvasGuideColor", "#d2ff5c7a").toString());
    m_snapGridGuideColor = QColor(settings.value("snapGridGuideColor", "#d256d3ff").toString());
    m_snapObjectGuideColor = QColor(settings.value("snapObjectGuideColor", "#dcffca58").toString());
}

// сеттеры
void ProjectManager::setBackgroundColor(const QColor &color)
{
    if (m_document.backgroundColor() != color) {
        m_document.setBackgroundColor(color);
        emit projectChanged();
    }
}

void ProjectManager::setCanvasSize(int width, int height)
{
    const int clampedWidth = qMax(1, width);
    const int clampedHeight = qMax(1, height);
    if (m_document.canvasWidth() != clampedWidth || m_document.canvasHeight() != clampedHeight) {
        m_document.setCanvasSize(clampedWidth, clampedHeight);
        emit projectChanged();
    }
}

void ProjectManager::setShowGrid(bool enabled)
{
    if (m_showGrid == enabled)
        return;
    m_showGrid = enabled;
    emit projectChanged();
}

void ProjectManager::setSnapToCanvas(bool enabled)
{
    if (m_snapToCanvas == enabled)
        return;
    m_snapToCanvas = enabled;
    emit projectChanged();
}

void ProjectManager::setSnapToGrid(bool enabled)
{
    if (m_snapToGrid == enabled)
        return;
    m_snapToGrid = enabled;
    emit projectChanged();
}

void ProjectManager::setSnapToObjects(bool enabled)
{
    if (m_snapToObjects == enabled)
        return;
    m_snapToObjects = enabled;
    emit projectChanged();
}

void ProjectManager::setEditorViewState(const EditorViewState &state)
{
    if (!qIsFinite(state.scale) || !qIsFinite(state.offset.x()) || !qIsFinite(state.offset.y())) return;
    EditorViewState next = state;
    next.scale = qBound(0.1, next.scale, 10.0);
    if (m_editorViewState.scale == next.scale && m_editorViewState.offset == next.offset
        && m_editorViewState.proportionalResize == next.proportionalResize && m_editorViewState.rotationSnapping == next.rotationSnapping) return;
    m_editorViewState = next;
    emit editorViewStateChanged();
}

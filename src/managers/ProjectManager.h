// ProjectManager - менеджер проекта, связывающий внутреннюю модель редактора с экспортом в XML.

#pragma once

#include <QColor>
#include <QDomDocument>
#include <QList>
#include <QObject>
#include <QSharedPointer>
#include <QStringList>
#include <optional>

#include "BaseObject.h"
#include "TextObject.h"
#include "EditorViewState.h"
#include "fpga/config/FpgaPipelineConfig.h"

class EditorProjectDocument;
class ThreeDViewObject;

// ObjectAlignment - направление выравнивания объектов относительно холста или выделения.
enum class ObjectAlignment
{
    Left,
    HCenter,
    Right,
    Top,
    VCenter,
    Bottom
};

// ProjectManager - менеджер проекта, связывающий внутреннюю модель редактора с экспортом в XML.
class ProjectManager : public QObject
{
    Q_OBJECT

public:
    // возвращает единственный экземпляр менеджера проекта
    static ProjectManager* instance();

    // загружает единый XML с аппаратными данными и метаданными редактора
    bool loadFromFile(const QString &fileName);
    // возвращает последнюю ошибку загрузки/сохранения для показа в интерфейсе
    QString lastErrorMessage() const;
    // создаёт новый пустой проект с заданной рабочей областью
    bool createNewProject(const QString &projectName, int width, int height, const QColor &backgroundColor, const QString &filePath = QString());
    // сохраняет текущий проект в XML; если путь не задан, использует текущий путь проекта
    bool saveToFile(const QString &targetFile = QString());
    // собирает строгий XML-формат для ПЛИС в памяти без записи на диск
    QDomDocument buildCompiledFpgaXml() const;
    // Сохраняет SystemVerilog и запоминает структуру только после успешной записи.
    bool exportFpgaConfiguration(const QString &path, const FpgaHardwareScene &scene, QString *error = nullptr);
    const FpgaPipelineConfig &fpgaConfiguration() const { return m_fpgaConfiguration; }
    // Импортирует изображение выбранного аппаратного типа и выделяет цветовые маски.
    int importImage(const QString &fileName, bool rotating = false);

    // регистрирует стандартные типы объектов редактора
    void registerStandardTypes();

    // создаёт объект по имени типа и помещает его в начало списка слоёв
    int addObject(const QString &typeName);
    // создаёт объект по имени типа и центрирует его в заданной точке холста
    int addObject(const QString &typeName, double x, double y);
    // добавляет существующий объект в редакторскую группу
    bool addObjectToGroup(int groupId, int index, bool recordChange = true);
    // удаляет набор объектов по индексам
    bool removeObjects(const QList<int> &indexes);
    // переставляет объекты в порядке старых индексов, переданных в order
    bool reorderObjects(const QList<int> &order);
    // переносит объекты ближе к переднему плану
    bool sendObjectsToFront(const QList<int> &indexes);
    // переносит объекты ближе к заднему плану
    bool sendObjectsToBack(const QList<int> &indexes);
    // создаёт редакторскую группу; группировка управляет удобством выбора и массовыми флагами
    int groupObjects(const QList<int> &indexes);
    // Превращает статические изображения/группы в один аппаратный staticgroup.
    int mergeStaticGroups(const QList<int> &indexes, QString *errorMessage = nullptr);
    // удаляет редакторские группы, содержащие выбранные объекты
    bool ungroupObjects(const QList<int> &indexes);
    // возвращает участников группы, если объект входит в редакторскую группу
    QList<int> groupMembersForObject(int index) const;
    // возвращает участников группы по идентификатору группы
    QList<int> groupMembers(int groupId) const;
    // возвращает имя группы по идентификатору
    QString groupName(int groupId) const;
    // переименовывает редакторскую группу
    bool renameGroup(int groupId, const QString &name);
    // задаёт видимость всех объектов редакторской группы
    bool setGroupViewVisible(int groupId, bool visible);
    bool setGroupSelectionLocked(int groupId, bool locked);
    // задаёт участие всех объектов редакторской группы в XML-экспорте
    bool setGroupExportEnabled(int groupId, bool enabled);
    // задаёт пользовательское имя объекта для списка объектов
    bool renameObject(int index, const QString &name);
    // копирует объекты во внутренний буфер обмена редактора
    bool copyObjects(const QList<int> &indexes);
    // вставляет объекты из внутреннего буфера и возвращает индексы новых объектов
    QList<int> pasteObjects(std::optional<QPointF> position = std::nullopt);
    // проверяет, есть ли объекты для вставки
    bool canPasteObjects() const;
    // отменяет последнее изменение проекта
    bool undo();
    // повторяет отменённое изменение проекта
    bool redo();
    // проверяет доступность undo
    bool canUndo() const;
    // проверяет доступность redo
    bool canRedo() const;
    // выравнивает одиночный объект относительно рабочей области
    bool alignObject(int index, ObjectAlignment alignment);
    // задаёт видимость объекта на холсте редактора
    bool setObjectViewVisible(int index, bool visible);
    bool setObjectSelectionLocked(int index, bool locked);
    // задаёт участие объекта в XML-экспорте
    bool setObjectExportEnabled(int index, bool enabled);
    // фиксирует начало интерактивного редактирования объекта для истории действий
    void recordObjectEdit();
    // завершает интерактивное редактирование объекта и записывает сообщение истории
    void finishObjectEdit(const QString &message = QString());
    // меняет свойство объекта через единую точку, чтобы сохранить историю действий
    bool setObjectProperty(BaseObject *object, const QString &propertyName, const QString &value);
    // Проверяем весь набор до изменения, затем записываем одну операцию истории.
    bool setObjectsProperty(const QList<QSharedPointer<BaseObject>> &objects, const QString &name, const QString &value);
    bool isObjectPropertyEditable(const BaseObject *object, const QString &name) const;

    // EditorLayer - имя, цвет выделения и идентификатор редакторского слоя.
    struct EditorLayer {
        int id = 1;
        QString name;
        QColor selectionColor;
    };
    const QList<EditorLayer> &editorLayers() const { return m_editorLayers; }
    int activeLayerId() const { return m_activeLayerId; }
    bool showAllLayers() const { return m_showAllLayers; }
    bool hasEditorLayer(int id) const;
    QString editorLayerName(int id) const;
    QColor editorLayerColor(int id) const;
    int layerObjectCount(int id) const;
    bool isObjectOnVisibleLayer(const BaseObject *object) const;
    bool isObjectVisibleInEditor(const BaseObject *object) const;
    int addEditorLayer();
    bool renameEditorLayer(int id, const QString &name);
    bool setEditorLayerColor(int id, const QColor &color);
    int duplicateEditorLayer(int id);
    bool mergeEditorLayer(int sourceId, int targetId);
    bool mergeEditorLayers(const QList<int> &layerIds, int targetId);
    bool removeEditorLayer(int id);
    bool setObjectsLayer(const QList<QSharedPointer<BaseObject>> &objects, int layerId);
    void setActiveLayer(int id);
    void setShowAllLayers(bool enabled);

    // возвращает плоский список объектов в порядке слоёв
    const QList<QSharedPointer<BaseObject>>& getObjects() const;
    // возвращает объект по индексу или nullptr при неверном индексе
    QSharedPointer<BaseObject> getObjectAt(int index) const;
    // возвращает количество объектов проекта
    int getObjectCount() const;
    // ObjectGroup - именованный набор объектов для выделения и совместного редактирования.
    // Сохраняется в <editor>, отдельно от аппаратных объектов.
    struct ObjectGroup
    {
        int id = -1;
        QString name;
        QList<int> members;
    };
    // возвращает список редакторских групп
    QList<ObjectGroup> objectGroups() const;
    // проверяет, добавлен ли в сцену редакторский объект 3D-вида
    bool hasThreeDViewObject() const;
    ThreeDViewObject *threeDViewObject() const;
    bool isAircraftStateChange() const { return m_aircraftStateChange; }
    quint64 sceneRevision() const { return m_sceneRevision; }
    bool setAircraftState(double x, double y, double altitude, double course, int screenY, bool recordChange = true);

    // возвращает имя проекта
    QString getProjectName() const;
    // возвращает ширину рабочей области
    int getCanvasWidth() const;
    // возвращает высоту рабочей области
    int getCanvasHeight() const;
    // возвращает цвет фона рабочей области
    QColor getBackgroundColor() const;
    // возвращает путь текущего XML-файла проекта
    QString getFilePath() const;
    // возвращает состояние отображения сетки
    bool showGrid() const;
    // возвращает состояние привязки к границам экрана
    bool snapToCanvas() const;
    // возвращает состояние привязки к сетке
    bool snapToGrid() const;
    // возвращает состояние привязки к другим объектам
    bool snapToObjects() const;

    // возвращает шаг сетки в пикселях
    int gridStep() const;
    // возвращает цвет сетки
    QColor gridColor() const;
    // возвращает цвет направляющих привязки к экрану
    QColor snapCanvasGuideColor() const;
    // возвращает цвет направляющих привязки к сетке
    QColor snapGridGuideColor() const;
    // возвращает цвет направляющих привязки к объектам
    QColor snapObjectGuideColor() const;
    // перечитывает глобальные настройки отображения и привязок
    void reloadGlobalSettings();

    // задаёт цвет фона рабочей области
    void setBackgroundColor(const QColor &color);
    // задаёт размер рабочей области
    void setCanvasSize(int width, int height);
    // включает или выключает отображение сетки
    void setShowGrid(bool enabled);
    // включает или выключает привязку к границам экрана
    void setSnapToCanvas(bool enabled);
    // включает или выключает привязку к сетке
    void setSnapToGrid(bool enabled);
    // включает или выключает привязку к объектам
    void setSnapToObjects(bool enabled);
    const EditorViewState &editorViewState() const { return m_editorViewState; }
    void setEditorViewState(const EditorViewState &state);

signals:
    void projectLoaded();
    void projectChanged();
    void logMessage(const QString &message);
    void editorLayersChanged();
    void editorLayerObjectsChanged();
    void editorLayerViewChanged();
    void editorViewStateChanged();

private:
    // ProjectSnapshot - снимок объектов и редакторских групп для undo/redo.
    struct ProjectSnapshot
    {
        QList<QSharedPointer<BaseObject>> objects;
        QStringList objectTags;
        QList<ObjectGroup> groups;
        int nextGroupId = 1;
        QList<EditorLayer> layers;
        int nextLayerId = 2;
        int activeLayerId = 1;
        bool showAllLayers = false;
    };

    ProjectManager();

    void recordHistory();
    ProjectSnapshot captureSnapshot() const;
    void restoreSnapshot(const ProjectSnapshot &snapshot);
    void clearHistory();
    void insertObjectAtFront(BaseObject *object, const QString &tagName);
    void syncNextGroupId();
    void resetEditorLayers();
    void notifyEditorLayersChanged();
    QString uniqueLayerName(const QString &base) const;
    void readGlobalWorkspaceSettings();

    EditorProjectDocument *m_document = nullptr;
    QString m_filePath;
    QString m_lastErrorMessage;
    QList<QSharedPointer<BaseObject>> m_objects;
    QStringList m_objectTags;
    QList<ObjectGroup> m_groups;
    int m_nextGroupId = 1;
    QList<EditorLayer> m_editorLayers;
    int m_nextLayerId = 2;
    int m_activeLayerId = 1;
    bool m_showAllLayers = false;
    EditorViewState m_editorViewState;
    FpgaPipelineConfig m_fpgaConfiguration;
    QMap<QString, ParamSchema> m_schemas;
    QMap<int, FpgaFont> m_fonts;
    QList<ProjectSnapshot> m_undoStack;
    QList<ProjectSnapshot> m_redoStack;
    QList<QSharedPointer<BaseObject>> m_clipboardObjects;
    QStringList m_clipboardTags;
    QList<ObjectGroup> m_clipboardGroups;
    quint64 m_sceneRevision = 0;
    bool m_aircraftStateChange = false;
    bool m_showGrid = false;
    bool m_snapToCanvas = true;
    bool m_snapToGrid = false;
    bool m_snapToObjects = false;

    int m_gridStep = 10;
    QColor m_gridColor = QColor(120, 180, 200, 55);
    QColor m_snapCanvasGuideColor = QColor(255, 92, 122, 210);
    QColor m_snapGridGuideColor = QColor(86, 211, 255, 210);
    QColor m_snapObjectGuideColor = QColor(255, 202, 88, 220);
};

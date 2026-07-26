//ProjectManager - менеджер проекта, связывающий внутреннюю модель редактора с экспортом в XML

#pragma once

#include <QColor>
#include <QList>
#include <QObject>
#include <QSharedPointer>
#include <QStringList>

#include "BaseObject.h"
#include "TextObject.h"

class EditorProjectDocument;

enum class ObjectAlignment
{
    Left,
    HCenter,
    Right,
    Top,
    VCenter,
    Bottom
};

class ProjectManager : public QObject
{
    Q_OBJECT

public:
    //возвращает единственный экземпляр менеджера проекта
    static ProjectManager* instance();

    //загружает проект из XML-файла редактора/ПЛИС
    bool loadFromFile(const QString &fileName);
    //создаёт новый пустой проект с заданной рабочей областью
    bool createNewProject(const QString &projectName, int width, int height, const QColor &backgroundColor, const QString &filePath = QString());
    //сохраняет текущий проект в XML; если путь не задан, использует текущий путь проекта
    bool saveToFile(const QString &targetFile = QString());
    //экспортирует текущий проект в строгий XML-формат для ПЛИС
    bool exportToFpgaXml(const QString &targetFile);
    //импортирует изображение как static_group с растровыми масками
    int importImageAsStaticGroup(const QString &fileName);

    //регистрирует стандартные типы объектов редактора
    void registerStandardTypes();

    //создаёт объект по имени типа и помещает его в начало списка слоёв
    int addObject(const QString &typeName);
    //создаёт объект по имени типа и центрирует его в заданной точке холста
    int addObject(const QString &typeName, double x, double y);
    //удаляет один объект по индексу
    bool removeObject(int index);
    //удаляет набор объектов по индексам
    bool removeObjects(const QList<int> &indexes);
    //переставляет объекты в порядке старых индексов, переданных в order
    bool reorderObjects(const QList<int> &order);
    //переносит объекты ближе к переднему плану
    bool sendObjectsToFront(const QList<int> &indexes);
    //переносит объекты ближе к заднему плану
    bool sendObjectsToBack(const QList<int> &indexes);
    //создаёт редакторскую группу; группировка управляет удобством выбора и массовыми флагами
    int groupObjects(const QList<int> &indexes);
    //удаляет редакторские группы, содержащие выбранные объекты
    bool ungroupObjects(const QList<int> &indexes);
    //возвращает участников группы, если объект входит в редакторскую группу
    QList<int> groupMembersForObject(int index) const;
    //возвращает участников группы по идентификатору группы
    QList<int> groupMembers(int groupId) const;
    //возвращает имя группы по идентификатору
    QString groupName(int groupId) const;
    //переименовывает редакторскую группу
    bool renameGroup(int groupId, const QString &name);
    //задаёт видимость всех объектов редакторской группы
    bool setGroupViewVisible(int groupId, bool visible);
    //задаёт участие всех объектов редакторской группы в XML-экспорте
    bool setGroupExportEnabled(int groupId, bool enabled);
    //задаёт пользовательское имя объекта для списка объектов
    bool renameObject(int index, const QString &name);
    //копирует объекты во внутренний буфер обмена редактора
    bool copyObjects(const QList<int> &indexes);
    //вставляет объекты из внутреннего буфера и возвращает индексы новых объектов
    QList<int> pasteObjects();
    //проверяет, есть ли объекты для вставки
    bool canPasteObjects() const;
    //отменяет последнее изменение проекта
    bool undo();
    //повторяет отменённое изменение проекта
    bool redo();
    //проверяет доступность undo
    bool canUndo() const;
    //проверяет доступность redo
    bool canRedo() const;
    //выравнивает одиночный объект относительно рабочей области
    bool alignObject(int index, ObjectAlignment alignment);
    //задаёт видимость объекта на холсте редактора
    bool setObjectViewVisible(int index, bool visible);
    //задаёт участие объекта в XML-экспорте
    bool setObjectExportEnabled(int index, bool enabled);
    //фиксирует начало интерактивного редактирования объекта для истории действий
    void recordObjectEdit();
    //завершает интерактивное редактирование объекта и записывает сообщение истории
    void finishObjectEdit(const QString &message = QString());
    //меняет свойство объекта через единую точку, чтобы сохранить историю действий
    bool setObjectProperty(BaseObject *object, const QString &propertyName, const QString &value);

    //возвращает плоский список объектов в порядке слоёв
    const QList<QSharedPointer<BaseObject>>& getObjects() const;
    //возвращает объект по индексу или nullptr при неверном индексе
    QSharedPointer<BaseObject> getObjectAt(int index) const;
    //возвращает количество объектов проекта
    int getObjectCount() const;
    //описание редакторской группы, не сериализуемой в аппаратный XML
    struct ObjectGroup
    {
        int id = -1;
        QString name;
        QList<int> members;
    };
    //возвращает список редакторских групп
    QList<ObjectGroup> objectGroups() const;

    //возвращает имя проекта
    QString getProjectName() const;
    //возвращает ширину рабочей области
    int getCanvasWidth() const;
    //возвращает высоту рабочей области
    int getCanvasHeight() const;
    //возвращает цвет фона рабочей области
    QColor getBackgroundColor() const;
    //возвращает путь текущего XML-файла проекта
    QString getFilePath() const;
    //возвращает состояние отображения сетки
    bool showGrid() const;
    //возвращает состояние привязки к границам экрана
    bool snapToCanvas() const;
    //возвращает состояние привязки к сетке
    bool snapToGrid() const;
    //возвращает состояние привязки к другим объектам
    bool snapToObjects() const;

    //возвращает шаг сетки в пикселях
    int gridStep() const;
    //возвращает цвет сетки
    QColor gridColor() const;
    //возвращает цвет направляющих привязки к экрану
    QColor snapCanvasGuideColor() const;
    //возвращает цвет направляющих привязки к сетке
    QColor snapGridGuideColor() const;
    //возвращает цвет направляющих привязки к объектам
    QColor snapObjectGuideColor() const;
    //перечитывает глобальные настройки отображения и привязок
    void reloadGlobalSettings();

    //задаёт цвет фона рабочей области
    void setBackgroundColor(const QColor &color);
    //задаёт размер рабочей области
    void setCanvasSize(int width, int height);
    //включает или выключает отображение сетки
    void setShowGrid(bool enabled);
    //включает или выключает привязку к границам экрана
    void setSnapToCanvas(bool enabled);
    //включает или выключает привязку к сетке
    void setSnapToGrid(bool enabled);
    //включает или выключает привязку к объектам
    void setSnapToObjects(bool enabled);

signals:
    void projectLoaded();
    void projectChanged();
    void logMessage(const QString &message);

private:
    struct ProjectSnapshot
    {
        QList<QSharedPointer<BaseObject>> objects;
        QStringList objectTags;
        QList<ObjectGroup> groups;
        int nextGroupId = 1;
    };

    ProjectManager();

    bool loadXmlProject(const QString &fileName);
    void applyRestrictedMode();
    void recordHistory();
    ProjectSnapshot captureSnapshot() const;
    void restoreSnapshot(const ProjectSnapshot &snapshot);
    void clearHistory();
    void insertObjectAtFront(BaseObject *object, const QString &tagName);

    EditorProjectDocument *m_document = nullptr;
    QString m_filePath;
    QList<QSharedPointer<BaseObject>> m_objects;
    QStringList m_objectTags;
    QList<ObjectGroup> m_groups;
    int m_nextGroupId = 1;
    QMap<QString, ParamSchema> m_schemas;
    QMap<QString, QString> m_schemaAliases;
    QMap<int, FpgaFont> m_fonts;
    QList<ProjectSnapshot> m_undoStack;
    QList<ProjectSnapshot> m_redoStack;
    QList<QSharedPointer<BaseObject>> m_clipboardObjects;
    QStringList m_clipboardTags;
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

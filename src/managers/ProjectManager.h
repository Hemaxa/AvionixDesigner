//ProjectManager - менеджер проекта, связывающий внутреннюю модель редактора с экспортом в XML

#pragma once

#include <QColor>
#include <QList>
#include <QObject>
#include <QSet>
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
    static ProjectManager* instance();

    bool loadFromFile(const QString &fileName);
    bool createNewProject(const QString &projectName, int width, int height, const QColor &backgroundColor, const QString &filePath = QString());
    bool saveToFile(const QString &targetFile = QString());
    bool exportToFpgaXml(const QString &targetFile, const QSet<QString> &alphabetGroups = {});
    int importImageAsStaticGroup(const QString &fileName);

    void registerStandardTypes();

    int addObject(const QString &typeName);
    int addObject(const QString &typeName, double x, double y);
    bool removeObject(int index);
    bool removeObjects(const QList<int> &indexes);
    bool reorderObjects(const QList<int> &order);
    bool sendObjectsToFront(const QList<int> &indexes);
    bool sendObjectsToBack(const QList<int> &indexes);
    bool copyObjects(const QList<int> &indexes);
    QList<int> pasteObjects();
    bool canPasteObjects() const;
    bool undo();
    bool redo();
    bool canUndo() const;
    bool canRedo() const;
    bool alignObject(int index, ObjectAlignment alignment);
    bool setObjectViewVisible(int index, bool visible);
    bool setObjectExportEnabled(int index, bool enabled);
    void recordObjectEdit();
    void finishObjectEdit(const QString &message = QString());
    bool setObjectProperty(BaseObject *object, const QString &propertyName, const QString &value);

    const QList<QSharedPointer<BaseObject>>& getObjects() const;
    QSharedPointer<BaseObject> getObjectAt(int index) const;
    int getObjectCount() const;

    QString getProjectName() const;
    int getCanvasWidth() const;
    int getCanvasHeight() const;
    QColor getBackgroundColor() const;
    QString getFilePath() const;
    bool showGrid() const;
    bool snapToCanvas() const;
    bool snapToGrid() const;
    bool snapToObjects() const;

    int gridStep() const;
    QColor gridColor() const;
    QColor snapCanvasGuideColor() const;
    QColor snapGridGuideColor() const;
    QColor snapObjectGuideColor() const;
    void reloadGlobalSettings();

    void setBackgroundColor(const QColor &color);
    void setCanvasSize(int width, int height);
    void setShowGrid(bool enabled);
    void setSnapToCanvas(bool enabled);
    void setSnapToGrid(bool enabled);
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

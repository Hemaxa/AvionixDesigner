//ProjectManager - менеджер проекта, связывающий внутреннюю модель редактора с экспортом в XML

#pragma once

#include <QColor>
#include <QList>
#include <QObject>
#include <QSet>
#include <QSharedPointer>

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

enum class ProjectEditMode
{
    EditableXml,
    RestrictedFpgaXml
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
    bool removeObject(int index);
    bool reorderObjects(const QList<int> &order);
    bool alignObject(int index, ObjectAlignment alignment);
    bool setObjectViewVisible(int index, bool visible);
    bool setObjectExportEnabled(int index, bool enabled);

    const QList<QSharedPointer<BaseObject>>& getObjects() const;
    QSharedPointer<BaseObject> getObjectAt(int index) const;
    int getObjectCount() const;

    QString getProjectName() const;
    int getCanvasWidth() const;
    int getCanvasHeight() const;
    QColor getBackgroundColor() const;
    QString getFilePath() const;
    ProjectEditMode editMode() const;
    QString editModeName() const;

    void setBackgroundColor(const QColor &color);
    void setCanvasSize(int width, int height);

signals:
    void projectLoaded();
    void projectChanged();
    void logMessage(const QString &message);

private:
    ProjectManager();

    bool loadXmlProject(const QString &fileName);
    bool saveEditableXml(const QString &targetFile);
    void applyRestrictedMode();
    void resetFontsToDefault();

    EditorProjectDocument *m_document = nullptr;
    QString m_filePath;
    ProjectEditMode m_editMode = ProjectEditMode::EditableXml;
    QList<QSharedPointer<BaseObject>> m_objects;
    QStringList m_objectTags;
    QMap<QString, ParamSchema> m_schemas;
    QMap<QString, QString> m_schemaAliases;
    QMap<int, FpgaFont> m_fonts;
};

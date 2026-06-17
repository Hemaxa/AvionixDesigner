//ProjectManager - менеджер проекта, связывающий внутреннюю модель редактора с экспортом в XML

#pragma once

#include <QColor>
#include <QList>
#include <QObject>
#include <QSharedPointer>

#include "BaseObject.h"

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

    void registerStandardTypes();

    int addObject(const QString &typeName);
    bool removeObject(int index);
    bool reorderObjects(const QList<int> &order);
    bool alignObject(int index, ObjectAlignment alignment);

    const QList<QSharedPointer<BaseObject>>& getObjects() const;
    QSharedPointer<BaseObject> getObjectAt(int index) const;
    int getObjectCount() const;

    QString getProjectName() const;
    int getCanvasWidth() const;
    int getCanvasHeight() const;
    QColor getBackgroundColor() const;
    QString getFilePath() const;

    void setBackgroundColor(const QColor &color);
    void setCanvasSize(int width, int height);

signals:
    void projectLoaded();
    void projectChanged();
    void logMessage(const QString &message);

private:
    ProjectManager();

    EditorProjectDocument *m_document = nullptr;
    QString m_filePath;
    QList<QSharedPointer<BaseObject>> m_objects;
    QStringList m_objectTags;
    QMap<QString, ParamSchema> m_schemas;
    QMap<QString, QString> m_schemaAliases;
};

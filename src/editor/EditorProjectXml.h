#pragma once

#include "EditorProjectDocument.h"
#include "EditorViewState.h"
#include "ProjectManager.h"

namespace EditorProjectXml {
// Полный редактируемый документ. Порядок objects — от переднего плана к заднему.
struct State
{
    EditorProjectDocument document;
    QList<QSharedPointer<BaseObject>> objects;
    QStringList objectTags;
    QList<ProjectManager::EditorLayer> layers;
    QList<ProjectManager::ObjectGroup> groups;
    int nextLayerId = 2;
    int nextGroupId = 1;
    int activeLayerId = 1;
    bool showAllLayers = false;
    bool showGrid = false;
    bool snapToCanvas = true;
    bool snapToGrid = false;
    bool snapToObjects = false;
    int gridStep = 10;
    QColor gridColor;
    QColor canvasGuideColor;
    QColor gridGuideColor;
    QColor objectGuideColor;
    EditorViewState view;
};

// Запись/чтение <editor version="1">. Чтение изменяет result только при полном успехе.
bool write(QDomDocument &document, const State &state, QString *error);
bool read(const QDomElement &editor, State *result, QString *error);
}

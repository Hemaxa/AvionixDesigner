// EditorProjectXml - обмен метаданными сцены между XML проекта и редактором.

#pragma once

#include "EditorProjectDocument.h"
#include "EditorViewState.h"
#include "ProjectManager.h"

namespace EditorProjectXml {
// State - восстановленные объекты, слои, группы и параметры рабочего пространства.
// Порядок objects — от переднего плана к заднему.
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
    FpgaPipelineConfig fpgaConfiguration;
};

// Запись/чтение <editor>. Аппаратные данные экспортируемых
// объектов берутся из objects по hardware-index. result изменяется только при успехе.
bool write(QDomDocument &document, const State &state, QString *error);
bool read(const QDomElement &editor, State *result, QString *error);
}

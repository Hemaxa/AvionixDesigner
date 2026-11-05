// EditorViewState - сохраняемое состояние просмотра и режимов преобразования проекта.

#pragma once

#include <QPointF>

// EditorViewState - масштаб, смещение холста и постоянные режимы изменения объектов.
// Жесты и временное выделение сюда не входят.
struct EditorViewState
{
    double scale = 1.0;
    QPointF offset;
    bool proportionalResize = false;
    bool rotationSnapping = false;
};

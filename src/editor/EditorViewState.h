#pragma once

#include <QPointF>

// Состояние просмотра конкретного проекта; жесты и временное выделение сюда не входят.
struct EditorViewState
{
    double scale = 1.0;
    QPointF offset;
    bool proportionalResize = false;
    bool rotationSnapping = false;
};

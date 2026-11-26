#pragma once

#include <QPointF>
#include <QRectF>

class BaseObject;

// CanvasGeometry — общие ограничения холста для мыши, клавиатуры и свойств.
// Обычные объекты крупнее холста выравниваются по его левому/верхнему краю;
// процедурные объекты могут задавать собственный canvasConstraintRect().
class CanvasGeometry
{
public:
    static QPointF constrainMove(const QRectF &bounds, const QPointF &delta, int width, int height);
    static void keepInside(BaseObject *object, int width, int height);
};

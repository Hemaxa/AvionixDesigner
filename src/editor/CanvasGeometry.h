#pragma once

#include <QPointF>
#include <QRectF>

class BaseObject;

// CanvasGeometry — общие ограничения холста для мыши, клавиатуры и свойств.
// Объекты крупнее холста выравниваются по его левому/верхнему краю.
class CanvasGeometry
{
public:
    static QPointF constrainMove(const QRectF &bounds, const QPointF &delta, int width, int height);
    static void keepInside(BaseObject *object, int width, int height);
};

#include "CanvasGeometry.h"
#include "BaseObject.h"

QPointF CanvasGeometry::constrainMove(const QRectF &bounds, const QPointF &delta, int width, int height)
{
    QRectF moved = bounds.translated(delta);
    QPointF constrained = delta;
    if (moved.width() <= width) {
        if (moved.left() < 0.0) constrained.rx() -= moved.left();
        else if (moved.right() > width) constrained.rx() -= moved.right() - width;
    } else constrained.rx() -= moved.left();

    moved = bounds.translated(constrained);
    if (moved.height() <= height) {
        if (moved.top() < 0.0) constrained.ry() -= moved.top();
        else if (moved.bottom() > height) constrained.ry() -= moved.bottom() - height;
    } else constrained.ry() -= moved.top();
    return constrained;
}

void CanvasGeometry::keepInside(BaseObject *object, int width, int height)
{
    if (!object || width <= 0 || height <= 0) return;
    const auto correction = constrainMove(object->getBoundingRect(), {}, width, height);
    if (!qFuzzyIsNull(correction.x()) || !qFuzzyIsNull(correction.y()))
        object->moveBy(correction.x(), correction.y());
}

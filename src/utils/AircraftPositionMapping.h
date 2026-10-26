#pragma once

#include <QPointF>
#include <QRectF>
#include <QtGlobal>

namespace AircraftPositionMapping {
constexpr int minPosition = -1000;
constexpr int maxPosition = 1000;

inline QPointF center(const QRectF &bounds, int x, int y, double markerSize = 0.0)
{
    const double normX = qBound(-1.0, x / double(maxPosition), 1.0);
    const double normY = qBound(-1.0, y / double(maxPosition), 1.0);
    const double travelX = qMin(bounds.width() * 0.38,
                               qMax(0.0, (bounds.width() - markerSize) * 0.5));
    const double travelY = qMin(bounds.height() * 0.38,
                               qMax(0.0, (bounds.height() - markerSize) * 0.5));
    return bounds.center() + QPointF(normX * travelX, -normY * travelY);
}
}

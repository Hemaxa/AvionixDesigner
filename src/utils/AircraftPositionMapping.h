// AircraftPositionMapping — перевод координат карты в экранное положение маркера.
#pragma once

#include <QPointF>
#include <QRectF>
#include <QString>
#include <QtGlobal>
#include "fpga/model/Layer3dFormat.h"

namespace AircraftPositionMapping {
constexpr int minPosition = Layer3dFormat::minPosition;
constexpr int maxPosition = Layer3dFormat::maxPosition;

inline QString coordinateLabel(double x, double y)
{
    return QStringLiteral("X: %1   Y: %2").arg(x, 0, 'f', 3).arg(y, 0, 'f', 3);
}

inline QPointF center(const QRectF &bounds, double x, double y, const QSizeF &markerSize)
{
    const double normX = qBound(double(minPosition), x, double(maxPosition)) / double(maxPosition);
    const double normY = qBound(double(minPosition), y, double(maxPosition)) / double(maxPosition);
    const double travelX = qMax(0.0, bounds.width() - markerSize.width());
    const double travelY = qMax(0.0, bounds.height() - markerSize.height());
    return bounds.center() + QPointF((normX - 0.5) * travelX, (0.5 - normY) * travelY);
}

inline QPointF center(const QRectF &bounds, double x, double y, double markerSize = 0.0)
{
    return center(bounds, x, y, QSizeF(markerSize, markerSize));
}
}

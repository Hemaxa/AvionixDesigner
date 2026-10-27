#pragma once

#include <QPointF>
#include <QRectF>
#include <QString>
#include <QtGlobal>
#include "fpga/model/Layer3dFormat.h"

namespace AircraftPositionMapping {
constexpr int minPosition = Layer3dFormat::minPosition;
constexpr int maxPosition = Layer3dFormat::maxPosition;

inline QString coordinateLabel(int x, int y)
{
    return QStringLiteral("X: %1   Y: %2").arg(x).arg(y);
}

inline QPointF center(const QRectF &bounds, int x, int y, const QSizeF &markerSize)
{
    const double normX = qBound(minPosition, x, maxPosition) / double(maxPosition);
    const double normY = qBound(minPosition, y, maxPosition) / double(maxPosition);
    const double travelX = qMax(0.0, bounds.width() - markerSize.width());
    const double travelY = qMax(0.0, bounds.height() - markerSize.height());
    return bounds.center() + QPointF((normX - 0.5) * travelX, (0.5 - normY) * travelY);
}

inline QPointF center(const QRectF &bounds, int x, int y, double markerSize = 0.0)
{
    return center(bounds, x, y, QSizeF(markerSize, markerSize));
}
}

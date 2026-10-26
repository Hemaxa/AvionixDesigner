#pragma once

#include <QPointF>
#include <QRectF>
#include <QString>
#include <QtGlobal>
#include <cmath>

namespace AircraftPositionMapping {
// Coordinates in layer3d are signed 17-bit values. Tiles are a display
// convention only: crossing a tile boundary must not reset these values.
constexpr int minPosition = -65536;
constexpr int maxPosition = 65535;
constexpr int defaultCellSize = 2000;
constexpr int minCellSize = 1;
constexpr int maxCellSize = 131072;

inline int boundedCellSize(int size)
{
    return qBound(minCellSize, size, maxCellSize);
}

inline int cellIndex(int coordinate, int size)
{
    size = boundedCellSize(size);
    // Tile [0; 0] is centered on the origin. Half-open intervals make the
    // positive and negative boundaries unambiguous, including odd sizes.
    return static_cast<int>(std::floor((coordinate + size * 0.5) / size));
}

inline double localPosition(int coordinate, int size)
{
    size = boundedCellSize(size);
    return coordinate - double(cellIndex(coordinate, size)) * size;
}

inline QString cellLabel(int x, int y, int size)
{
    return QStringLiteral("Квадрат [%1; %2]")
        .arg(cellIndex(x, size)).arg(cellIndex(y, size));
}

inline QString coordinateLabel(int x, int y)
{
    return QStringLiteral("X: %1   Y: %2").arg(x).arg(y);
}

inline QPointF center(const QRectF &bounds, int x, int y, double markerSize = 0.0,
                      int cellSize = defaultCellSize)
{
    cellSize = boundedCellSize(cellSize);
    const double normX = localPosition(x, cellSize) / (cellSize * 0.5);
    const double normY = localPosition(y, cellSize) / (cellSize * 0.5);
    const double travelX = qMin(bounds.width() * 0.38,
                               qMax(0.0, (bounds.width() - markerSize) * 0.5));
    const double travelY = qMin(bounds.height() * 0.38,
                               qMax(0.0, (bounds.height() - markerSize) * 0.5));
    return bounds.center() + QPointF(normX * travelX, -normY * travelY);
}
}

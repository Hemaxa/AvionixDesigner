#pragma once

#include <QtGlobal>
#include <cmath>

// tests/protocol/All_together.py / SlowParameters. Coordinates are map points, NOT meters.
namespace Layer3dFormat {
constexpr int xOffset = 0;
constexpr int yOffset = 19;
constexpr int courseOffset = 38;
constexpr int heightOffset = 57;
constexpr int coordinateBits = 19;
constexpr int courseBits = 19;
constexpr int heightBits = 17;
constexpr int screenYOffset = 74;
constexpr int screenYBits = 10;
constexpr int paddingOffset = 84;
constexpr int paddingBits = 66;
constexpr int hexLength = 38;
constexpr int coordinateFractionBits = 6;
constexpr int coordinateUnitsPerPoint = 1 << coordinateFractionBits;
constexpr double metersPerPoint = 32.0;
constexpr int minPosition = 0;
constexpr int maxPosition = 8191; // editor working area, including fractional movement
constexpr int maxCoordinateRaw = (1 << coordinateBits) - 1;
constexpr int minScreenY = 0;
constexpr int maxScreenY = (1 << screenYBits) - 1;
constexpr int defaultScreenY = 1023;
constexpr double defaultHeight = 16000.0;
constexpr double defaultCourse = 45.0;
constexpr int courseTurn = 1 << courseBits;
constexpr double courseScale = courseTurn / 360.0;
constexpr int heightUnitsPerMeter = 4;
constexpr int minHeightRaw = -(1 << (heightBits - 1));
constexpr int maxHeightRaw = (1 << (heightBits - 1)) - 1;
constexpr double minHeight = minHeightRaw / double(heightUnitsPerMeter);
constexpr double maxHeight = maxHeightRaw / double(heightUnitsPerMeter);
constexpr double heightStep = 1.0 / heightUnitsPerMeter;

// Python round(): ties to even, independent of the CPU rounding mode.
inline qint64 roundRaw(double value)
{
    const double lower = std::floor(value);
    const double fraction = value - lower;
    return qint64(lower + (fraction > 0.5 || (fraction == 0.5 && std::fmod(lower, 2.0) != 0.0)));
}
inline double normalizedCourse(double degrees)
{
    double result = std::fmod(degrees, 360.0);
    return result < 0 ? result + 360.0 : result;
}
inline quint32 courseToRaw(double degrees)
{
    if (!qIsFinite(degrees)) return 0;
    if (std::abs(degrees) > 1e12) degrees = normalizedCourse(degrees);
    return quint32(roundRaw(degrees * courseScale)) & (courseTurn - 1);
}
inline quint32 heightToRaw(double meters)
{
    if (!qIsFinite(meters)) return 0;
    return quint32(roundRaw(qBound(minHeight, meters, maxHeight) * heightUnitsPerMeter)) & ((1 << heightBits) - 1);
}
inline quint32 positionToRaw(double points)
{
    if (!qIsFinite(points)) return 0;
    return quint32(roundRaw(qBound(double(minPosition), points, double(maxPosition)) * coordinateUnitsPerPoint));
}
inline double movementInPoints(double speedKmh, double seconds)
{
    return speedKmh / 3.6 * seconds / metersPerPoint;
}
static_assert(xOffset + coordinateBits == yOffset);
static_assert(yOffset + coordinateBits == courseOffset);
static_assert(courseOffset + courseBits == heightOffset);
static_assert(heightOffset + heightBits == screenYOffset);
static_assert(screenYOffset + screenYBits == paddingOffset);
static_assert(paddingOffset + paddingBits == 150);
}

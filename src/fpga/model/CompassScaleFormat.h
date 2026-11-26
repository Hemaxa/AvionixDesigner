// CompassScaleFormat — документированная раскладка COMPASS_SCALE из ПараметрыШкал.pdf.
#pragma once

#include "ScaleFormat.h"

namespace CompassScaleFormat {
constexpr int reservedOffset = 0;
constexpr int reservedBits = 2;
constexpr int yOffset = 2;
constexpr int coordinateBits = 12;
constexpr int courseOffset = 14;
constexpr int courseBits = 17;
constexpr int colorOffset = 31;
constexpr int colorBits = 24;
constexpr int paddingOffset = 55;
constexpr int paddingBits = 95;
constexpr double minCourse = -512.0;
constexpr double maxCourse = 511.9921875;

// Константы рисунка из PDF. Не входят в iparam и не создают растровую память.
constexpr int fixedStepQ4 = 105;
constexpr int stroke = 2;
constexpr int textWidth = 32;
constexpr int textHeight = 21;

inline quint32 course(double value)
{
    if (!std::isfinite(value)) return 0;
    return quint32(ScaleFormat::roundEven(qBound(minCourse, value, maxCourse) * 128.0)) & 0x1ffffu;
}
}

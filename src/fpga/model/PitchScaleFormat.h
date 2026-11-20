// PitchScaleFormat — раскладка PITCH_SCALE из docs/pitch_scale_control_raw.py.
#pragma once

#include <QtGlobal>
#include <cmath>

namespace PitchScaleFormat {
constexpr int enableOffset = 0;
constexpr int modeOffset = 1;
constexpr int xOffset = 2;
constexpr int yOffset = 14;
constexpr int pitchOffset = 26;
constexpr int rotationOffset = 42;
constexpr int paddingOffset = 58;
constexpr int coordinateBits = 12;
constexpr int angleBits = 16;
constexpr int paddingBits = 92;

// Python round(): половина округляется к ближайшему чётному, включая отрицательные числа.
// Не зависит от текущего режима округления процессора.
inline int roundEven(double value)
{
    const double lower = std::floor(value);
    const double fraction = value - lower;
    const int integer = static_cast<int>(lower);
    return integer + (fraction > 0.5 || (fraction == 0.5 && integer % 2 != 0) ? 1 : 0);
}

inline quint32 coordinate(double value)
{
    if (!std::isfinite(value)) return 0;
    return static_cast<quint32>(roundEven(qBound(0.0, value, 4095.0)));
}

inline quint32 angle(double value)
{
    if (!std::isfinite(value)) return 0;
    value = qBound(-256.0, value, 255.9921875);
    return static_cast<quint32>(roundEven(value * 128.0)) & 0xffffu;
}

inline double quantizedAngle(double value)
{
    return roundEven(value * 128.0) / 128.0;
}
}

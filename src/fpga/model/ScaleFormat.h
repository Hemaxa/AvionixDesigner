// ScaleFormat — общее округление целых координат и углов Q7 для шкал Python.
#pragma once

#include <QtGlobal>
#include <cmath>

namespace ScaleFormat {
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

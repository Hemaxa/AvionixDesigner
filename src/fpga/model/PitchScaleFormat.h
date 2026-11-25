// PitchScaleFormat — раскладка PITCH_SCALE из docs/pitch_scale_control_raw.py.
#pragma once

#include "ScaleFormat.h"

namespace PitchScaleFormat {
// Фиксированный рисунок для 1920×1080, независимый от размера холста.
constexpr int previewWidth = 280;
constexpr int previewHeight = 1080;

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

using ScaleFormat::roundEven;
using ScaleFormat::coordinate;
using ScaleFormat::angle;
using ScaleFormat::quantizedAngle;
}

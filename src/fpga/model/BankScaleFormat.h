// BankScaleFormat — раскладка BANK_SCALE из docs/scale_control_raw.py.
#pragma once

namespace BankScaleFormat {
constexpr int enableOffset = 0;
constexpr int reservedFlagOffset = 1;
constexpr int xOffset = 2;
constexpr int yOffset = 14;
constexpr int reservedAngleOffset = 26;
constexpr int bankOffset = 42;
constexpr int radiusOffset = 58;
constexpr int tickColorOffset = 70;
constexpr int mark20ColorOffset = 94;
constexpr int colorValidOffset = 118;
constexpr int paddingOffset = 119;
constexpr int coordinateBits = 12;
constexpr int angleBits = 16;
constexpr int colorBits = 24;
constexpr int paddingBits = 31;

// Размеры для 1920×1080. В Python radius помечен ref; плата пока не меняет геометрию.
constexpr int fixedRadius = 78;
}

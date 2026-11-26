// BankScaleFormat — раскладка BANK_SCALE из docs/Все, что есть по шкалам/scale_control_raw.py.
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

// Поле radius из Python и фиксированная геометрия PDF описаны независимо.
constexpr int defaultRadius = 78;
constexpr int fixedRadius = 173;
constexpr int tickLength = 24;
constexpr int tickWidth = 5;
constexpr int arrowOffset = 70;
constexpr int labelCell = 3;
}

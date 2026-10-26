#pragma once

// Bit positions in the 150-bit layer3d word, counted from its least
// significant bit. XML uses 38 hex digits; UART sends 19 bytes little-endian.
namespace Layer3dFormat {
constexpr int xOffset = 0;
constexpr int yOffset = 19;
constexpr int courseOffset = 38;
constexpr int heightOffset = 57;
constexpr int coordinateBits = 19;
constexpr int courseBits = 19;
constexpr int heightBits = 17;
constexpr int paddingOffset = 74;
constexpr int paddingBits = 76;
constexpr int hexLength = 38;
constexpr int minPosition = -(1 << (coordinateBits - 1));
constexpr int maxPosition = (1 << (coordinateBits - 1)) - 1;
constexpr int courseTurn = 1 << courseBits;
constexpr int heightUnitsPerMeter = 4;
constexpr int minHeightRaw = -(1 << (heightBits - 1));
constexpr int maxHeightRaw = (1 << (heightBits - 1)) - 1;
}

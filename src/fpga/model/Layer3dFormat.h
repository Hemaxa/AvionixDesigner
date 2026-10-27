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
constexpr int pitchOffset = 74;
constexpr int pitchBits = 10;
constexpr int paddingOffset = 84;
constexpr int paddingBits = 66;
constexpr int hexLength = 38;
constexpr int coordinateFractionBits = 6;
constexpr int coordinateUnitsPerMeter = 1 << coordinateFractionBits;
constexpr int minPosition = 0;
constexpr int maxPosition = (1 << (coordinateBits - coordinateFractionBits)) - 1;
constexpr int minPitch = -(1 << (pitchBits - 1));
constexpr int maxPitch = (1 << (pitchBits - 1)) - 1;
constexpr int courseTurn = 1 << courseBits;
constexpr int heightUnitsPerMeter = 4;
constexpr int minHeightRaw = -(1 << (heightBits - 1));
constexpr int maxHeightRaw = (1 << (heightBits - 1)) - 1;
}

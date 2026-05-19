#pragma once

#include <array>
#include <cstdint>

namespace gb {

inline constexpr uint16_t kRegIf = 0xFF0F;
inline constexpr uint16_t kRegIe = 0xFFFF;

inline constexpr uint8_t kIntVBlank = 1u << 0;
inline constexpr uint8_t kIntStat = 1u << 1;
inline constexpr uint8_t kIntTimer = 1u << 2;
inline constexpr uint8_t kIntSerial = 1u << 3;
inline constexpr uint8_t kIntJoypad = 1u << 4;
inline constexpr uint8_t kIntMask = 0x1F;

// priority order is bit order, vblank first
inline constexpr std::array<uint16_t, 5> kInterruptVectors = {0x40, 0x48, 0x50, 0x58, 0x60};

} // namespace gb

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

// one shared line: components set bits, the bus maps 0xFF0F onto it
class InterruptLine {
public:
    void request(uint8_t mask) {
        if_ = static_cast<uint8_t>((if_ | mask) & kIntMask);
    }
    uint8_t read() const {
        return if_;
    }
    void write(uint8_t value) {
        if_ = static_cast<uint8_t>(value & kIntMask);
    }

private:
    // pandocs power-up: if reads 0xE1
    uint8_t if_ = 0x01;
};

} // namespace gb

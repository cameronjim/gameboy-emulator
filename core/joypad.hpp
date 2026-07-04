#pragma once

#include <cstdint>

namespace gb {

inline constexpr uint16_t kRegJoyp = 0xFF00;

// bit index doubles as the pressed_ bit: low nibble directions, high nibble actions
enum class Button : uint8_t { Right = 0, Left, Up, Down, A, B, Select, Start };

class Joypad {
public:
    void set_button(Button b, bool pressed);
    uint8_t read() const;
    void write(uint8_t value) {
        select_ = static_cast<uint8_t>(value & 0x30);
    }

private:
    // both groups deselected at power-up
    uint8_t select_ = 0x30;
    uint8_t pressed_ = 0x00;
};

} // namespace gb

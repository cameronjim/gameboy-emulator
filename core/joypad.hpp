#pragma once

#include "state.hpp"

#include <cstddef>
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

    static constexpr size_t kStateSize = 2;
    void save_state(StateWriter& w) const {
        w.u8(select_);
        w.u8(pressed_);
    }
    void load_state(StateReader& r) {
        select_ = static_cast<uint8_t>(r.u8() & 0x30);
        pressed_ = r.u8();
    }

private:
    // both groups deselected at power-up
    uint8_t select_ = 0x30;
    uint8_t pressed_ = 0x00;
};

} // namespace gb

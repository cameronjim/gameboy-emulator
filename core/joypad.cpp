#include "joypad.hpp"

namespace gb {

void Joypad::set_button(Button b, bool pressed) {
    const uint8_t mask = static_cast<uint8_t>(1u << static_cast<uint8_t>(b));
    if (pressed) {
        pressed_ = static_cast<uint8_t>(pressed_ | mask);
    } else {
        pressed_ = static_cast<uint8_t>(pressed_ & ~mask);
    }
}

uint8_t Joypad::read() const {
    // active low: pressed reads 0, unselected group and unpressed bits read 1
    uint8_t low = 0x0F;
    if ((select_ & 0x10) == 0) {
        low = static_cast<uint8_t>(low & ~(pressed_ & 0x0F));
    }
    if ((select_ & 0x20) == 0) {
        low = static_cast<uint8_t>(low & ~((pressed_ >> 4) & 0x0F));
    }
    return static_cast<uint8_t>(0xC0 | select_ | low);
}

} // namespace gb

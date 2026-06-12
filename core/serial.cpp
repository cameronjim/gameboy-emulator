#include "serial.hpp"

namespace gb {

void Serial::write_sc(uint8_t value) {
    sc_ = value;
    // start bit with internal clock: emit instantly and clear the start bit
    if (value == 0x81) {
        if (sink_) {
            sink_(sb_);
        }
        sc_ = static_cast<uint8_t>(sc_ & 0x7F);
    }
}

} // namespace gb

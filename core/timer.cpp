#include "timer.hpp"

#include <array>

namespace gb {

namespace {
// tac select 0..3 -> counter bit; the frequency order is not monotonic
constexpr std::array<uint8_t, 4> kTacBit = {9, 3, 5, 7};
} // namespace

bool Timer::signal() const {
    if ((tac_ & 0x04) == 0) {
        return false;
    }
    return ((counter_ >> kTacBit[tac_ & 0x03]) & 1u) != 0;
}

void Timer::tick(uint32_t tcycles) {
    for (uint32_t i = 0; i < tcycles; ++i) {
        const bool old_signal = signal();
        ++counter_;
        if (old_signal && !signal()) {
            increment_tima();
        }
    }
}

void Timer::write_div() {
    const bool old_signal = signal();
    // pandocs: any div write clears the whole counter
    counter_ = 0;
    if (old_signal && !signal()) {
        increment_tima();
    }
}

void Timer::write_tac(uint8_t value) {
    const bool old_signal = signal();
    tac_ = static_cast<uint8_t>(value & 0x07);
    if (old_signal && !signal()) {
        increment_tima();
    }
}

void Timer::increment_tima() {
    ++tima_;
    if (tima_ == 0) {
        // v1: reload on the overflow tick, the one-m-cycle 0x00 window is deferred
        tima_ = tma_;
        irq_.request(kIntTimer);
    }
}

} // namespace gb

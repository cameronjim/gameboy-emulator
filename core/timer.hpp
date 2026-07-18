#pragma once

#include "interrupts.hpp"
#include "state.hpp"

#include <cstddef>
#include <cstdint>

namespace gb {

inline constexpr uint16_t kRegDiv = 0xFF04;
inline constexpr uint16_t kRegTima = 0xFF05;
inline constexpr uint16_t kRegTma = 0xFF06;
inline constexpr uint16_t kRegTac = 0xFF07;

class Timer {
public:
    explicit Timer(InterruptLine& irq) : irq_(irq) {}

    // called per instruction with elapsed t-cycles, never per frame
    void tick(uint32_t tcycles);

    uint8_t read_div() const {
        return static_cast<uint8_t>(counter_ >> 8);
    }
    void write_div();
    uint8_t read_tima() const {
        return tima_;
    }
    void write_tima(uint8_t value) {
        tima_ = value;
    }
    uint8_t read_tma() const {
        return tma_;
    }
    void write_tma(uint8_t value) {
        tma_ = value;
    }
    uint8_t read_tac() const {
        // unused bits 3-7 read 1
        return static_cast<uint8_t>(tac_ | 0xF8);
    }
    void write_tac(uint8_t value);

    static constexpr size_t kStateSize = 5;
    void save_state(StateWriter& w) const {
        w.u16(counter_);
        w.u8(tima_);
        w.u8(tma_);
        w.u8(tac_);
    }
    void load_state(StateReader& r) {
        counter_ = r.u16();
        tima_ = r.u8();
        tma_ = r.u8();
        tac_ = static_cast<uint8_t>(r.u8() & 0x07);
    }

private:
    bool signal() const;
    void increment_tima();

    InterruptLine& irq_;
    // gekkio: dmg post-boot internal counter, div high byte reads 0xab
    uint16_t counter_ = 0xABCC;
    uint8_t tima_ = 0x00;
    uint8_t tma_ = 0x00;
    uint8_t tac_ = 0x00;
};

} // namespace gb

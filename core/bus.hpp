#pragma once

#include "apu.hpp"
#include "interrupts.hpp"
#include "joypad.hpp"
#include "mapper.hpp"
#include "memory.hpp"
#include "ppu.hpp"
#include "serial.hpp"
#include "state.hpp"
#include "timer.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace gb {

class Bus final : public Memory {
public:
    Bus(Serial& serial, Timer& timer, Ppu& ppu, Apu& apu, Joypad& joypad, InterruptLine& irq)
        : serial_(serial), timer_(timer), ppu_(ppu), apu_(apu), joypad_(joypad), irq_(irq) {}

    void attach_mapper(Mapper& mapper) {
        mapper_ = &mapper;
    }

    uint8_t read8(uint16_t addr) override;
    void write8(uint16_t addr, uint8_t value) override;
    uint16_t read16(uint16_t addr);
    void write16(uint16_t addr, uint16_t value);

    static constexpr size_t kStateSize = 0x2000 + 0x7F + 2;
    void save_state(StateWriter& w) const {
        w.bytes(wram_);
        w.bytes(hram_);
        w.u8(ie_);
        w.u8(dma_);
    }
    void load_state(StateReader& r) {
        r.bytes(wram_);
        r.bytes(hram_);
        ie_ = r.u8();
        dma_ = r.u8();
    }

private:
    uint8_t read_io(uint16_t addr);
    void write_io(uint16_t addr, uint8_t value);

    Serial& serial_;
    Timer& timer_;
    Ppu& ppu_;
    Apu& apu_;
    Joypad& joypad_;
    InterruptLine& irq_;
    // non-owning, attached at rom load
    Mapper* mapper_ = nullptr;
    std::array<uint8_t, 0x2000> wram_{};
    std::array<uint8_t, 0x7F> hram_{};
    uint8_t ie_ = 0x00;
    // pandocs power-up: dma reads 0xFF on dmg
    uint8_t dma_ = 0xFF;
};

} // namespace gb

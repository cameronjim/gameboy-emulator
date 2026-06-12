#pragma once

#include "mapper.hpp"
#include "memory.hpp"
#include "serial.hpp"

#include <array>
#include <cstdint>

namespace gb {

class Bus final : public Memory {
public:
    explicit Bus(Serial& serial) : serial_(serial) {}

    void attach_mapper(Mapper& mapper) {
        mapper_ = &mapper;
    }

    uint8_t read8(uint16_t addr) override;
    void write8(uint16_t addr, uint8_t value) override;
    uint16_t read16(uint16_t addr);
    void write16(uint16_t addr, uint16_t value);

private:
    uint8_t read_io(uint16_t addr);
    void write_io(uint16_t addr, uint8_t value);

    Serial& serial_;
    // non-owning, attached at rom load
    Mapper* mapper_ = nullptr;
    std::array<uint8_t, 0x2000> vram_{};
    std::array<uint8_t, 0x2000> wram_{};
    std::array<uint8_t, 0xA0> oam_{};
    std::array<uint8_t, 0x7F> hram_{};
    // pandocs power-up: if reads 0xE1
    uint8_t if_ = 0x01;
    uint8_t ie_ = 0x00;
    // pandocs power-up: dma reads 0xFF on dmg
    uint8_t dma_ = 0xFF;
};

} // namespace gb

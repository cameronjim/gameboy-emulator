#pragma once

#include <cstdint>

namespace gb {

class Mapper {
public:
    virtual ~Mapper() = default;
    virtual uint8_t read_rom(uint16_t addr) const = 0;
    virtual void write_rom(uint16_t addr, uint8_t value) = 0;
    virtual uint8_t read_ram(uint16_t addr) const = 0;
    virtual void write_ram(uint16_t addr, uint8_t value) = 0;
};

} // namespace gb

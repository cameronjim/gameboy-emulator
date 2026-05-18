#pragma once

#include "mapper.hpp"

#include <cstdint>
#include <vector>

namespace gb {

class MapperRom final : public Mapper {
public:
    explicit MapperRom(std::vector<uint8_t> rom);
    uint8_t read_rom(uint16_t addr) const override;
    void write_rom(uint16_t addr, uint8_t value) override;
    uint8_t read_ram(uint16_t addr) const override;
    void write_ram(uint16_t addr, uint8_t value) override;

private:
    std::vector<uint8_t> rom_;
};

} // namespace gb

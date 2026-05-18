#include "mapper_rom.hpp"

#include <utility>

namespace gb {

MapperRom::MapperRom(std::vector<uint8_t> rom) : rom_(std::move(rom)) {}

uint8_t MapperRom::read_rom(uint16_t addr) const {
    if (rom_.empty()) {
        return 0xFF;
    }
    // masked index is always in bounds, mirrors hardware address wrap
    return rom_[addr & (rom_.size() - 1)];
}

void MapperRom::write_rom(uint16_t, uint8_t) {}

uint8_t MapperRom::read_ram(uint16_t) const {
    // no cart ram: open bus
    return 0xFF;
}

void MapperRom::write_ram(uint16_t, uint8_t) {}

} // namespace gb

#include "mapper_mbc1.hpp"

#include <utility>

namespace gb {

MapperMbc1::MapperMbc1(std::vector<uint8_t> rom, uint32_t ram_size)
    : rom_(std::move(rom)), ram_(ram_size, 0) {}

uint8_t MapperMbc1::read_rom(uint16_t addr) const {
    if (rom_.empty()) {
        return 0xFF;
    }
    uint32_t bank;
    if (addr < 0x4000) {
        // mode 1 remaps the lower region through bank2
        bank = mode_ != 0 ? static_cast<uint32_t>(bank2_) << 5 : 0;
    } else {
        bank = (static_cast<uint32_t>(bank2_) << 5) | bank1_;
    }
    // mask to the real bank count
    bank &= rom_bank_count() - 1;
    return rom_[bank * 0x4000 + (addr & 0x3FFF)];
}

void MapperMbc1::write_rom(uint16_t addr, uint8_t value) {
    if (addr < 0x2000) {
        ram_enable_ = (value & 0x0F) == 0x0A;
    } else if (addr < 0x4000) {
        // writing 0 selects 1, checked before masking
        bank1_ = static_cast<uint8_t>(value & 0x1F);
        if (bank1_ == 0) {
            bank1_ = 1;
        }
    } else if (addr < 0x6000) {
        bank2_ = static_cast<uint8_t>(value & 0x03);
    } else {
        mode_ = static_cast<uint8_t>(value & 0x01);
    }
}

uint8_t MapperMbc1::read_ram(uint16_t addr) const {
    if (!ram_enable_ || ram_.empty()) {
        return 0xFF;
    }
    const uint32_t bank = mode_ != 0 ? bank2_ : 0;
    return ram_[(bank * 0x2000 + addr) % ram_.size()];
}

void MapperMbc1::write_ram(uint16_t addr, uint8_t value) {
    if (!ram_enable_ || ram_.empty()) {
        return;
    }
    const uint32_t bank = mode_ != 0 ? bank2_ : 0;
    ram_[(bank * 0x2000 + addr) % ram_.size()] = value;
}

} // namespace gb

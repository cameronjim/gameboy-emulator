#include "mapper_mbc3.hpp"

#include <utility>

namespace gb {

MapperMbc3::MapperMbc3(std::vector<uint8_t> rom, uint32_t ram_size)
    : rom_(std::move(rom)), ram_(ram_size, 0) {}

uint8_t MapperMbc3::read_rom(uint16_t addr) const {
    if (rom_.empty()) {
        return 0xFF;
    }
    uint32_t bank = 0;
    if (addr >= 0x4000) {
        bank = rom_bank_ & (rom_bank_count() - 1);
    }
    return rom_[bank * 0x4000 + (addr & 0x3FFF)];
}

void MapperMbc3::write_rom(uint16_t addr, uint8_t value) {
    if (addr < 0x2000) {
        ram_enable_ = (value & 0x0F) == 0x0A;
    } else if (addr < 0x4000) {
        // full 7 bits, writing 0 selects 1
        rom_bank_ = static_cast<uint8_t>(value & 0x7F);
        if (rom_bank_ == 0) {
            rom_bank_ = 1;
        }
    } else if (addr < 0x6000) {
        select_ = value;
    } else {
        // latch on a 0x00 -> 0x01 write pair
        if (last_latch_ == 0x00 && value == 0x01) {
            rtc_latched_ = rtc_;
        }
        last_latch_ = value;
    }
}

uint8_t MapperMbc3::read_ram(uint16_t addr) const {
    if (!ram_enable_) {
        return 0xFF;
    }
    if (select_ <= 0x03) {
        if (ram_.empty()) {
            return 0xFF;
        }
        return ram_[(static_cast<uint32_t>(select_) * 0x2000 + addr) % ram_.size()];
    }
    if (select_ >= 0x08 && select_ <= 0x0C) {
        return rtc_latched_[select_ - 0x08];
    }
    return 0xFF;
}

void MapperMbc3::write_ram(uint16_t addr, uint8_t value) {
    if (!ram_enable_) {
        return;
    }
    if (select_ <= 0x03) {
        if (ram_.empty()) {
            return;
        }
        ram_[(static_cast<uint32_t>(select_) * 0x2000 + addr) % ram_.size()] = value;
        return;
    }
    if (select_ >= 0x08 && select_ <= 0x0C) {
        rtc_[select_ - 0x08] = value;
    }
}

void MapperMbc3::set_rtc_seconds(uint64_t seconds) {
    // v1: static time-of-load clock, tick-accurate rtc deferred
    rtc_[0] = static_cast<uint8_t>(seconds % 60);
    rtc_[1] = static_cast<uint8_t>((seconds / 60) % 60);
    rtc_[2] = static_cast<uint8_t>((seconds / 3600) % 24);
    const uint64_t days = seconds / 86400;
    rtc_[3] = static_cast<uint8_t>(days & 0xFF);
    rtc_[4] = static_cast<uint8_t>((days >> 8) & 0x01);
    rtc_latched_ = rtc_;
}

} // namespace gb

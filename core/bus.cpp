#include "bus.hpp"

#include "interrupts.hpp"

namespace gb {

namespace {
constexpr uint16_t kRegDma = 0xFF46;
} // namespace

uint8_t Bus::read8(uint16_t addr) {
    if (addr <= 0x7FFF) {
        return mapper_ != nullptr ? mapper_->read_rom(addr) : 0xFF;
    }
    if (addr <= 0x9FFF) {
        return vram_[addr - 0x8000];
    }
    if (addr <= 0xBFFF) {
        return mapper_ != nullptr ? mapper_->read_ram(static_cast<uint16_t>(addr - 0xA000)) : 0xFF;
    }
    if (addr <= 0xDFFF) {
        return wram_[addr - 0xC000];
    }
    if (addr <= 0xFDFF) {
        // echo ram mirrors wram
        return wram_[addr - 0xE000];
    }
    if (addr <= 0xFE9F) {
        return oam_[addr - 0xFE00];
    }
    if (addr <= 0xFEFF) {
        // unusable region
        return 0xFF;
    }
    if (addr <= 0xFF7F) {
        return read_io(addr);
    }
    if (addr <= 0xFFFE) {
        return hram_[addr - 0xFF80];
    }
    return ie_;
}

void Bus::write8(uint16_t addr, uint8_t value) {
    if (addr <= 0x7FFF) {
        if (mapper_ != nullptr) {
            mapper_->write_rom(addr, value);
        }
        return;
    }
    if (addr <= 0x9FFF) {
        vram_[addr - 0x8000] = value;
        return;
    }
    if (addr <= 0xBFFF) {
        if (mapper_ != nullptr) {
            mapper_->write_ram(static_cast<uint16_t>(addr - 0xA000), value);
        }
        return;
    }
    if (addr <= 0xDFFF) {
        wram_[addr - 0xC000] = value;
        return;
    }
    if (addr <= 0xFDFF) {
        wram_[addr - 0xE000] = value;
        return;
    }
    if (addr <= 0xFE9F) {
        oam_[addr - 0xFE00] = value;
        return;
    }
    if (addr <= 0xFEFF) {
        return;
    }
    if (addr <= 0xFF7F) {
        write_io(addr, value);
        return;
    }
    if (addr <= 0xFFFE) {
        hram_[addr - 0xFF80] = value;
        return;
    }
    ie_ = value;
}

uint16_t Bus::read16(uint16_t addr) {
    const uint8_t lo = read8(addr);
    const uint8_t hi = read8(static_cast<uint16_t>(addr + 1));
    return static_cast<uint16_t>((hi << 8) | lo);
}

void Bus::write16(uint16_t addr, uint16_t value) {
    write8(addr, static_cast<uint8_t>(value & 0xFF));
    write8(static_cast<uint16_t>(addr + 1), static_cast<uint8_t>(value >> 8));
}

uint8_t Bus::read_io(uint16_t addr) {
    switch (addr) {
    case kRegSb:
        return serial_.read_sb();
    case kRegSc:
        return serial_.read_sc();
    case kRegIf:
        // upper 3 bits read as 1
        return static_cast<uint8_t>(0xE0 | if_);
    case kRegDma:
        return dma_;
    default:
        // unmapped io reads 0xFF, never 0x00
        return 0xFF;
    }
}

void Bus::write_io(uint16_t addr, uint8_t value) {
    switch (addr) {
    case kRegSb:
        serial_.write_sb(value);
        break;
    case kRegSc:
        serial_.write_sc(value);
        break;
    case kRegIf:
        if_ = static_cast<uint8_t>(value & kIntMask);
        break;
    case kRegDma: {
        dma_ = value;
        // v1 decision: instant copy of 160 bytes from xx00
        const uint16_t src = static_cast<uint16_t>(value << 8);
        for (uint8_t i = 0; i < 0xA0; ++i) {
            oam_[i] = read8(static_cast<uint16_t>(src + i));
        }
        break;
    }
    default:
        break;
    }
}

} // namespace gb

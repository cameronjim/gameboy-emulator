#include "ppu.hpp"

namespace gb {

namespace {
constexpr uint32_t kDotsPerLine = 456;
constexpr uint8_t kLinesPerFrame = 154;
constexpr uint8_t kFirstVBlankLine = 144;
// v1: fixed mode 3 length
constexpr uint32_t kOamDots = 80;
constexpr uint32_t kDrawEndDot = 80 + 172;
constexpr uint8_t kStatHBlankEnable = 0x08;
constexpr uint8_t kStatVBlankEnable = 0x10;
constexpr uint8_t kStatOamEnable = 0x20;
constexpr uint8_t kStatLycEnable = 0x40;
} // namespace

PpuMode Ppu::compute_mode() const {
    if (ly_ >= kFirstVBlankLine) {
        return PpuMode::VBlank;
    }
    if (dot_ < kOamDots) {
        return PpuMode::OamScan;
    }
    if (dot_ < kDrawEndDot) {
        return PpuMode::Drawing;
    }
    return PpuMode::HBlank;
}

void Ppu::tick(uint32_t tcycles) {
    if (!lcd_enabled()) {
        return;
    }
    dot_ += tcycles;
    while (dot_ >= kDotsPerLine) {
        dot_ -= kDotsPerLine;
        ly_ = static_cast<uint8_t>((ly_ + 1) % kLinesPerFrame);
        if (ly_ == kFirstVBlankLine) {
            irq_.request(kIntVBlank);
            if ((stat_enables_ & kStatVBlankEnable) != 0) {
                irq_.request(kIntStat);
            }
        }
        compare_lyc();
    }
    const PpuMode mode = compute_mode();
    if (mode != mode_) {
        enter_mode(mode);
    }
}

void Ppu::enter_mode(PpuMode mode) {
    mode_ = mode;
    switch (mode) {
    case PpuMode::Drawing:
        render_scanline();
        break;
    case PpuMode::HBlank:
        if ((stat_enables_ & kStatHBlankEnable) != 0) {
            irq_.request(kIntStat);
        }
        break;
    case PpuMode::OamScan:
        if ((stat_enables_ & kStatOamEnable) != 0) {
            irq_.request(kIntStat);
        }
        break;
    case PpuMode::VBlank:
        break;
    }
}

void Ppu::compare_lyc() {
    if (ly_ == lyc_ && (stat_enables_ & kStatLycEnable) != 0) {
        irq_.request(kIntStat);
    }
}

uint8_t Ppu::read_register(uint16_t addr) const {
    switch (addr) {
    case kRegLcdc:
        return lcdc_;
    case kRegStat: {
        const uint8_t lyc_flag = ly_ == lyc_ ? 0x04 : 0x00;
        const uint8_t mode_bits = lcd_enabled() ? static_cast<uint8_t>(mode_) : 0;
        // bit 7 unused, reads 1
        return static_cast<uint8_t>(0x80 | stat_enables_ | lyc_flag | mode_bits);
    }
    case kRegScy:
        return scy_;
    case kRegScx:
        return scx_;
    case kRegLy:
        return ly_;
    case kRegLyc:
        return lyc_;
    case kRegBgp:
        return bgp_;
    default:
        return 0xFF;
    }
}

void Ppu::write_register(uint16_t addr, uint8_t value) {
    switch (addr) {
    case kRegLcdc: {
        const bool was_enabled = lcd_enabled();
        lcdc_ = value;
        if (was_enabled && !lcd_enabled()) {
            // lcd off: ly and mode reset, no ppu interrupts
            ly_ = 0;
            dot_ = 0;
            mode_ = PpuMode::HBlank;
        }
        break;
    }
    case kRegStat:
        stat_enables_ = static_cast<uint8_t>(value & 0x78);
        break;
    case kRegScy:
        scy_ = value;
        break;
    case kRegScx:
        scx_ = value;
        break;
    case kRegLy:
        // read-only
        break;
    case kRegLyc:
        lyc_ = value;
        compare_lyc();
        break;
    case kRegBgp:
        bgp_ = value;
        break;
    default:
        break;
    }
}

void Ppu::render_scanline() {
    uint8_t* row = &framebuffer_[static_cast<uint32_t>(ly_) * kLcdWidth];
    if ((lcdc_ & 0x01) == 0) {
        // bg disabled draws blank white
        for (uint32_t x = 0; x < kLcdWidth; ++x) {
            row[x] = 0;
        }
        return;
    }
    const uint16_t map_base = (lcdc_ & 0x08) != 0 ? 0x1C00 : 0x1800;
    const bool unsigned_mode = (lcdc_ & 0x10) != 0;
    const uint8_t bgy = static_cast<uint8_t>(scy_ + ly_);
    const uint8_t row_in_tile = bgy & 7;
    for (uint32_t x = 0; x < kLcdWidth; ++x) {
        const uint8_t bgx = static_cast<uint8_t>(scx_ + x);
        const uint8_t tile = vram_[map_base + (bgy / 8) * 32 + (bgx / 8)];
        uint16_t tile_addr;
        if (unsigned_mode) {
            tile_addr = static_cast<uint16_t>(tile * 16);
        } else {
            // lcdc bit 4 clear: signed indexing from 0x9000
            tile_addr = static_cast<uint16_t>(0x1000 + static_cast<int8_t>(tile) * 16);
        }
        const uint8_t lo = vram_[tile_addr + row_in_tile * 2];
        const uint8_t hi = vram_[tile_addr + row_in_tile * 2 + 1];
        // bit 7 is the leftmost pixel
        const uint8_t px = bgx & 7;
        const uint8_t color = static_cast<uint8_t>((((hi >> (7 - px)) & 1) << 1) | ((lo >> (7 - px)) & 1));
        row[x] = static_cast<uint8_t>((bgp_ >> (color * 2)) & 0x03);
    }
}

} // namespace gb

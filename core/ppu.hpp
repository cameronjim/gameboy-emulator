#pragma once

#include "interrupts.hpp"
#include "state.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace gb {

inline constexpr uint32_t kLcdWidth = 160;
inline constexpr uint32_t kLcdHeight = 144;

inline constexpr uint16_t kRegLcdc = 0xFF40;
inline constexpr uint16_t kRegStat = 0xFF41;
inline constexpr uint16_t kRegScy = 0xFF42;
inline constexpr uint16_t kRegScx = 0xFF43;
inline constexpr uint16_t kRegLy = 0xFF44;
inline constexpr uint16_t kRegLyc = 0xFF45;
inline constexpr uint16_t kRegBgp = 0xFF47;
inline constexpr uint16_t kRegObp0 = 0xFF48;
inline constexpr uint16_t kRegObp1 = 0xFF49;
inline constexpr uint16_t kRegWy = 0xFF4A;
inline constexpr uint16_t kRegWx = 0xFF4B;

enum class PpuMode : uint8_t { HBlank = 0, VBlank = 1, OamScan = 2, Drawing = 3 };

class Ppu {
public:
    explicit Ppu(InterruptLine& irq) : irq_(irq) {}

    // called per instruction with elapsed t-cycles
    void tick(uint32_t tcycles);

    uint8_t read_vram(uint16_t offset) const {
        return vram_[offset];
    }
    void write_vram(uint16_t offset, uint8_t value) {
        vram_[offset] = value;
    }
    uint8_t read_oam(uint16_t offset) const {
        return oam_[offset];
    }
    void write_oam(uint16_t offset, uint8_t value) {
        oam_[offset] = value;
    }

    uint8_t read_register(uint16_t addr) const;
    void write_register(uint16_t addr, uint8_t value);

    std::span<const uint8_t> framebuffer() const {
        return framebuffer_;
    }
    // debug accessor for the tile viewer
    std::span<const uint8_t> vram() const {
        return vram_;
    }
    PpuMode mode() const {
        return mode_;
    }

    static constexpr size_t kStateSize = 0x2000 + 0xA0 + 4 + 13;
    void save_state(StateWriter& w) const;
    void load_state(StateReader& r);

private:
    bool lcd_enabled() const {
        return (lcdc_ & 0x80) != 0;
    }
    PpuMode compute_mode() const;
    void enter_mode(PpuMode mode);
    void compare_lyc();
    void render_scanline();
    void render_bg(std::span<uint8_t> colors) const;
    bool render_window(std::span<uint8_t> colors) const;
    void render_sprites(std::span<const uint8_t> colors, std::span<uint8_t> row) const;

    InterruptLine& irq_;
    std::array<uint8_t, 0x2000> vram_{};
    std::array<uint8_t, 0xA0> oam_{};
    std::array<uint8_t, kLcdWidth * kLcdHeight> framebuffer_{};
    uint32_t dot_ = 0;
    uint8_t ly_ = 0;
    uint8_t lyc_ = 0;
    // pandocs power-up values
    uint8_t lcdc_ = 0x91;
    uint8_t bgp_ = 0xFC;
    uint8_t scy_ = 0;
    uint8_t scx_ = 0;
    // hardware leaves obp undefined at power-up
    uint8_t obp0_ = 0x00;
    uint8_t obp1_ = 0x00;
    uint8_t wy_ = 0;
    uint8_t wx_ = 0;
    // internal window line, advances only on lines the window rendered
    uint8_t window_line_ = 0;
    // writable stat bits 3-6 only
    uint8_t stat_enables_ = 0;
    PpuMode mode_ = PpuMode::OamScan;
};

} // namespace gb

#include "ppu.hpp"

#include "interrupts.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

namespace {

struct Rig {
    gb::InterruptLine irq;
    gb::Ppu ppu{irq};

    Rig() {
        irq.write(0);
    }
    uint8_t stat_mode() {
        return static_cast<uint8_t>(ppu.read_register(gb::kRegStat) & 0x03);
    }
    uint8_t ly() {
        return ppu.read_register(gb::kRegLy);
    }
    // write one bg tile row: tile index, row, bitplanes
    void set_tile_row(uint16_t tile_base, uint8_t row, uint8_t lo, uint8_t hi) {
        ppu.write_vram(static_cast<uint16_t>(tile_base + row * 2), lo);
        ppu.write_vram(static_cast<uint16_t>(tile_base + row * 2 + 1), hi);
    }
    // render line 0 by ticking into mode 3
    void render_line0() {
        ppu.tick(80 + 1);
    }
};

} // namespace

TEST_CASE("mode_sequence_and_dot_budgets_per_line") {
    Rig rig;
    REQUIRE(rig.stat_mode() == 2);
    rig.ppu.tick(80);
    REQUIRE(rig.stat_mode() == 3);
    rig.ppu.tick(172);
    REQUIRE(rig.stat_mode() == 0);
    rig.ppu.tick(204);
    REQUIRE(rig.ly() == 1);
    REQUIRE(rig.stat_mode() == 2);
}

TEST_CASE("ly_increments_and_wraps_at_154") {
    Rig rig;
    rig.ppu.tick(456 * 153);
    REQUIRE(rig.ly() == 153);
    rig.ppu.tick(456);
    REQUIRE(rig.ly() == 0);
}

TEST_CASE("vblank_interrupt_at_line_144") {
    Rig rig;
    rig.ppu.tick(456 * 144);
    REQUIRE(rig.ly() == 144);
    REQUIRE(rig.stat_mode() == 1);
    REQUIRE((rig.irq.read() & gb::kIntVBlank) != 0);
}

TEST_CASE("lyc_coincidence_sets_stat_bit_and_interrupts_when_enabled") {
    Rig rig;
    rig.ppu.write_register(gb::kRegLyc, 5);
    rig.ppu.write_register(gb::kRegStat, 0x40);
    rig.ppu.tick(456 * 4);
    REQUIRE((rig.ppu.read_register(gb::kRegStat) & 0x04) == 0);
    REQUIRE((rig.irq.read() & gb::kIntStat) == 0);
    rig.ppu.tick(456);
    REQUIRE((rig.ppu.read_register(gb::kRegStat) & 0x04) != 0);
    REQUIRE((rig.irq.read() & gb::kIntStat) != 0);
}

TEST_CASE("bg_scanline_unsigned_addressing") {
    Rig rig;
    // lcdc bit 4 set: tile 1 lives at 0x8010
    rig.ppu.write_register(gb::kRegLcdc, 0x91);
    rig.ppu.write_register(gb::kRegBgp, 0xE4);
    rig.ppu.write_vram(0x1800, 1);
    rig.set_tile_row(0x0010, 0, 0xFF, 0x00);
    rig.render_line0();
    REQUIRE(rig.ppu.framebuffer()[0] == 1);
    REQUIRE(rig.ppu.framebuffer()[7] == 1);
    REQUIRE(rig.ppu.framebuffer()[8] == 0);
}

TEST_CASE("bg_scanline_signed_addressing") {
    Rig rig;
    // lcdc bit 4 clear: index 0xff resolves to 0x9000 - 16 = 0x8ff0
    rig.ppu.write_register(gb::kRegLcdc, 0x81);
    rig.ppu.write_register(gb::kRegBgp, 0xE4);
    rig.ppu.write_vram(0x1800, 0xFF);
    rig.set_tile_row(0x0FF0, 0, 0x00, 0xFF);
    rig.render_line0();
    REQUIRE(rig.ppu.framebuffer()[0] == 2);
    REQUIRE(rig.ppu.framebuffer()[7] == 2);
    REQUIRE(rig.ppu.framebuffer()[8] == 0);
}

TEST_CASE("scx_scy_wraparound") {
    Rig rig;
    rig.ppu.write_register(gb::kRegLcdc, 0x91);
    rig.ppu.write_register(gb::kRegBgp, 0xE4);
    // tile at map cell (31, 31), bottom-right corner of the 256x256 bg
    rig.ppu.write_vram(0x1800 + 31 * 32 + 31, 1);
    rig.set_tile_row(0x0010, 7, 0xFF, 0x00);
    // scroll so bg (248..255, 255) lands at screen x 0..7 of line 0
    rig.ppu.write_register(gb::kRegScx, 248);
    rig.ppu.write_register(gb::kRegScy, 255);
    rig.render_line0();
    REQUIRE(rig.ppu.framebuffer()[0] == 1);
    REQUIRE(rig.ppu.framebuffer()[7] == 1);
    // x 8 wraps to bg x 0, an empty tile
    REQUIRE(rig.ppu.framebuffer()[8] == 0);
}

TEST_CASE("bitplane_merge_bit7_is_leftmost") {
    Rig rig;
    rig.ppu.write_register(gb::kRegLcdc, 0x91);
    rig.ppu.write_register(gb::kRegBgp, 0xE4);
    rig.ppu.write_vram(0x1800, 1);
    // lo bit7 and hi bit7 set: leftmost pixel color 3, rest 0
    rig.set_tile_row(0x0010, 0, 0x80, 0x80);
    rig.render_line0();
    REQUIRE(rig.ppu.framebuffer()[0] == 3);
    REQUIRE(rig.ppu.framebuffer()[1] == 0);
}

TEST_CASE("bgp_palette_applies") {
    Rig rig;
    rig.ppu.write_register(gb::kRegLcdc, 0x91);
    // map color 1 to shade 3
    rig.ppu.write_register(gb::kRegBgp, 0x0C);
    rig.ppu.write_vram(0x1800, 1);
    rig.set_tile_row(0x0010, 0, 0xFF, 0x00);
    rig.render_line0();
    REQUIRE(rig.ppu.framebuffer()[0] == 3);
}

TEST_CASE("lcd_off_resets_ly_and_mode") {
    Rig rig;
    rig.ppu.tick(456 * 10);
    REQUIRE(rig.ly() == 10);
    rig.ppu.write_register(gb::kRegLcdc, 0x11);
    REQUIRE(rig.ly() == 0);
    REQUIRE(rig.stat_mode() == 0);
    rig.ppu.tick(456 * 5);
    REQUIRE(rig.ly() == 0);
    // no ppu interrupts while off
    REQUIRE(rig.irq.read() == 0);
    rig.ppu.write_register(gb::kRegLcdc, 0x91);
    rig.ppu.tick(456);
    REQUIRE(rig.ly() == 1);
}

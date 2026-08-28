#include "cartridge.hpp"
#include "gameboy.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

std::vector<uint8_t> read_crossy_rom() {
    std::ifstream in(CROSSY_ROM_PATH, std::ios::binary);
    REQUIRE(in.good());
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

size_t count_lit_pixels(std::span<const uint8_t> fb) {
    size_t lit = 0;
    for (uint8_t shade : fb) {
        if (shade != 0) {
            ++lit;
        }
    }
    return lit;
}

constexpr uint32_t kBootFrames = 120;

// screen rows of the hover screen's three text lines
constexpr uint32_t kTitleRow = 6;
constexpr uint32_t kPromptRow = 10;
constexpr uint32_t kBestRow = 12;

// leftmost and rightmost bg cell columns on a tile row whose id is in [lo, hi]
std::pair<int, int> glyph_span(const gb::Gameboy& gameboy, uint32_t row, uint8_t lo, uint8_t hi) {
    const std::span<const uint16_t> ids = gameboy.framebuffer_tiles();
    int left = -1;
    int right = -1;
    for (uint32_t cx = 0; cx < 20; ++cx) {
        const uint16_t id = ids[(row * 8 + 3) * gb::kLcdWidth + cx * 8 + 3];
        if ((id & 0x100u) != 0) {
            continue;
        }
        const uint8_t tile = static_cast<uint8_t>(id);
        if (tile >= lo && tile <= hi) {
            if (left < 0) {
                left = static_cast<int>(cx);
            }
            right = static_cast<int>(cx);
        }
    }
    return {left, right};
}

} // namespace

TEST_CASE("crossy_rom_declares_an_mbc1_battery_cart") {
    const std::vector<uint8_t> rom = read_crossy_rom();
    REQUIRE(rom.size() == 32768u);

    std::string why;
    auto cart = gb::Cartridge::parse(rom, &why);
    REQUIRE(why.empty());
    REQUIRE(cart.has_value());
    REQUIRE(cart->type() == gb::CartType::Mbc1);
    REQUIRE(cart->has_battery());
    REQUIRE(cart->ram_size() > 0u);
    REQUIRE(cart->title() == "CROSSY");
}

TEST_CASE("crossy_boots_to_a_non_blank_title_card") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    REQUIRE(gameboy.load_rom(rom));
    for (uint32_t i = 0; i < kBootFrames; ++i) {
        gameboy.run_frame();
    }
    REQUIRE(count_lit_pixels(gameboy.framebuffer()) > 100u);
}

TEST_CASE("crossy_boot_is_deterministic") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy first;
    gb::Gameboy second;
    REQUIRE(first.load_rom(rom));
    REQUIRE(second.load_rom(rom));
    for (uint32_t i = 0; i < kBootFrames; ++i) {
        first.run_frame();
        second.run_frame();
    }

    const std::span<const uint8_t> a = first.framebuffer();
    const std::span<const uint8_t> b = second.framebuffer();
    REQUIRE(a.size() == b.size());
    REQUIRE(std::equal(a.begin(), a.end(), b.begin()));
}

TEST_CASE("crossy_title_text_is_centered") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    REQUIRE(gameboy.load_rom(rom));
    for (uint32_t i = 0; i < kBootFrames; ++i) {
        gameboy.run_frame();
    }

    // even-length lines land symmetric around the 20 column grid: left + right == 19
    for (uint32_t row : {kTitleRow, kPromptRow, kBestRow}) {
        const auto [left, right] = glyph_span(gameboy, row, 0x01, 0x5F);
        REQUIRE(left >= 0);
        REQUIRE(left + right == 19);
    }
}

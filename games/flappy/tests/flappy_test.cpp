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
#include <vector>

namespace {

std::vector<uint8_t> read_flappy_rom() {
    std::ifstream in(FLAPPY_ROM_PATH, std::ios::binary);
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

// the bird is the only sprite; ppu marks sprite pixels with bit 8 of the tile id
size_t count_sprite_pixels(const gb::Gameboy& gameboy) {
    const std::span<const uint16_t> ids = gameboy.framebuffer_tiles();
    const std::span<const uint8_t> fb = gameboy.framebuffer();
    size_t lit = 0;
    for (size_t i = 0; i < ids.size(); ++i) {
        if ((ids[i] & 0x100u) != 0 && fb[i] != 0) {
            ++lit;
        }
    }
    return lit;
}

constexpr int kNoBird = -1;

// screen y of the bird's topmost sprite pixel
int bird_y(const gb::Gameboy& gameboy) {
    const std::span<const uint16_t> ids = gameboy.framebuffer_tiles();
    const std::span<const uint8_t> fb = gameboy.framebuffer();
    for (size_t i = 0; i < ids.size(); ++i) {
        if ((ids[i] & 0x100u) != 0 && fb[i] != 0) {
            return static_cast<int>(i / gb::kLcdWidth);
        }
    }
    return kNoBird;
}

void press(gb::Gameboy& gameboy, gb::Button button, uint32_t frames) {
    gameboy.set_button(button, true);
    for (uint32_t i = 0; i < frames; ++i) {
        gameboy.run_frame();
    }
    gameboy.set_button(button, false);
}

void run(gb::Gameboy& gameboy, uint32_t frames) {
    for (uint32_t i = 0; i < frames; ++i) {
        gameboy.run_frame();
    }
}

// boots the rom and leaves it a few frames into the play state
void start_play(gb::Gameboy& gameboy, const std::vector<uint8_t>& rom) {
    REQUIRE(gameboy.load_rom(rom));
    run(gameboy, kBootFrames);
    press(gameboy, gb::Button::Start, 2);
    run(gameboy, 2);
}

} // namespace

TEST_CASE("flappy_rom_declares_an_mbc1_battery_cart") {
    const std::vector<uint8_t> rom = read_flappy_rom();
    REQUIRE(rom.size() == 32768u);

    std::string why;
    auto cart = gb::Cartridge::parse(rom, &why);
    REQUIRE(why.empty());
    REQUIRE(cart.has_value());
    REQUIRE(cart->type() == gb::CartType::Mbc1);
    REQUIRE(cart->has_battery());
    REQUIRE(cart->ram_size() > 0u);
    REQUIRE(cart->title() == "FLAPPY");
}

TEST_CASE("flappy_boots_to_a_non_blank_title_card") {
    const std::vector<uint8_t> rom = read_flappy_rom();

    gb::Gameboy gameboy;
    REQUIRE(gameboy.load_rom(rom));
    for (uint32_t i = 0; i < kBootFrames; ++i) {
        gameboy.run_frame();
    }
    REQUIRE(count_lit_pixels(gameboy.framebuffer()) > 100u);
}

TEST_CASE("flappy_boot_is_deterministic") {
    const std::vector<uint8_t> rom = read_flappy_rom();

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

TEST_CASE("title_has_no_sprites") {
    const std::vector<uint8_t> rom = read_flappy_rom();

    gb::Gameboy gameboy;
    REQUIRE(gameboy.load_rom(rom));
    run(gameboy, kBootFrames);
    REQUIRE(count_sprite_pixels(gameboy) == 0u);
}

TEST_CASE("start_spawns_the_bird") {
    const std::vector<uint8_t> rom = read_flappy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    run(gameboy, 30);
    REQUIRE(count_sprite_pixels(gameboy) > 0u);
    REQUIRE(bird_y(gameboy) != kNoBird);
}

TEST_CASE("bird_falls_without_input") {
    const std::vector<uint8_t> rom = read_flappy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    const int before = bird_y(gameboy);
    REQUIRE(before != kNoBird);
    run(gameboy, 30);
    REQUIRE(bird_y(gameboy) > before);
}

TEST_CASE("flap_lifts_the_bird") {
    const std::vector<uint8_t> rom = read_flappy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    run(gameboy, 30);
    const int before = bird_y(gameboy);
    REQUIRE(before != kNoBird);

    press(gameboy, gb::Button::A, 1);
    int highest = before;
    for (uint32_t i = 0; i < 20; ++i) {
        gameboy.run_frame();
        highest = std::min(highest, bird_y(gameboy));
    }
    REQUIRE(highest < before);
}

TEST_CASE("bird_stays_on_screen") {
    const std::vector<uint8_t> rom = read_flappy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    for (uint32_t i = 0; i < 600; ++i) {
        gameboy.run_frame();
        const int y = bird_y(gameboy);
        REQUIRE(y != kNoBird);
        REQUIRE(y >= 0);
        REQUIRE(y < static_cast<int>(gb::kLcdHeight));
    }
}

TEST_CASE("holding_a_does_not_autofire") {
    const std::vector<uint8_t> rom = read_flappy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    std::vector<int> ys;
    gameboy.set_button(gb::Button::A, true);
    for (uint32_t i = 0; i < 60; ++i) {
        gameboy.run_frame();
        ys.push_back(bird_y(gameboy));
    }
    gameboy.set_button(gb::Button::A, false);

    const size_t apex = static_cast<size_t>(std::min_element(ys.begin(), ys.end()) - ys.begin());
    REQUIRE(apex + 20 < ys.size());
    // the single flap dies out: 20 frames past the apex the bird is lower again
    REQUIRE(ys[apex + 20] > ys[apex]);
    // a second flap would show up as a dip; after the apex the bird only ever sinks
    for (size_t i = apex + 1; i < ys.size(); ++i) {
        REQUIRE(ys[i] >= ys[i - 1]);
    }
    REQUIRE(ys.back() > ys[apex]);
}

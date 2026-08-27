#include "cartridge.hpp"
#include "gameboy.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
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

constexpr uint8_t kBirdTileId = 0xE0;
constexpr uint8_t kDigitTileId = 0xD0;

// ppu marks sprite pixels with bit 8 of the tile id
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

// the bird and the score digits are separate sprite tiles, so tests must say which
size_t count_sprite_pixels_of(const gb::Gameboy& gameboy, uint8_t tile) {
    const std::span<const uint16_t> ids = gameboy.framebuffer_tiles();
    const std::span<const uint8_t> fb = gameboy.framebuffer();
    size_t lit = 0;
    for (size_t i = 0; i < ids.size(); ++i) {
        if ((ids[i] & 0x100u) != 0 && static_cast<uint8_t>(ids[i]) == tile && fb[i] != 0) {
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
        if ((ids[i] & 0x100u) != 0 && static_cast<uint8_t>(ids[i]) == kBirdTileId && fb[i] != 0) {
            return static_cast<int>(i / gb::kLcdWidth);
        }
    }
    return kNoBird;
}

bool hud_shows_digit(const gb::Gameboy& gameboy, uint8_t digit) {
    return count_sprite_pixels_of(gameboy, static_cast<uint8_t>(kDigitTileId + digit)) > 0;
}

// font glyph cells only ever reach the screen through the game over banner
bool banner_has_glyph(const gb::Gameboy& gameboy, uint8_t tile) {
    for (uint16_t id : gameboy.framebuffer_tiles()) {
        if ((id & 0x100u) == 0 && static_cast<uint8_t>(id) == tile) {
            return true;
        }
    }
    return false;
}

// gbdk's ibm font lands ascii 0x20-0x7f on tiles 0x00-0x5f
constexpr uint8_t font_tile(char c) {
    return static_cast<uint8_t>(c - 0x20);
}

bool banner_has_nonzero_digit(const gb::Gameboy& gameboy) {
    for (char c = '1'; c <= '9'; ++c) {
        if (banner_has_glyph(gameboy, font_tile(c))) {
            return true;
        }
    }
    return false;
}

constexpr size_t kSaveBestOffset = 4;

uint16_t sram_best(std::span<const uint8_t> ram) {
    return static_cast<uint16_t>(ram[kSaveBestOffset] | (ram[kSaveBestOffset + 1] << 8));
}

bool sram_has_magic(std::span<const uint8_t> ram) {
    return ram.size() > kSaveBestOffset + 1 && ram[0] == 'F' && ram[1] == 'L' && ram[2] == 'P' &&
           ram[3] == 'Y';
}

constexpr int kNoPipe = -1;
constexpr int kGroundTopPx = 128;
constexpr int kBirdSizePx = 8;
constexpr int kBirdScreenX = 40;

bool is_pipe_tile(uint16_t id) {
    return (id & 0x100u) == 0 && (id & 0xFFu) >= 0xA0u && (id & 0xFFu) <= 0xA3u;
}

// screen x of the leftmost pipe pixel on the whole screen
int leftmost_pipe_x(const gb::Gameboy& gameboy) {
    const std::span<const uint16_t> ids = gameboy.framebuffer_tiles();
    for (size_t x = 0; x < gb::kLcdWidth; ++x) {
        for (size_t y = 0; y < gb::kLcdHeight; ++y) {
            if (is_pipe_tile(ids[y * gb::kLcdWidth + x])) {
                return static_cast<int>(x);
            }
        }
    }
    return kNoPipe;
}

size_t count_ground_pixels(const gb::Gameboy& gameboy) {
    const std::span<const uint16_t> ids = gameboy.framebuffer_tiles();
    size_t found = 0;
    for (size_t y = kGroundTopPx; y < gb::kLcdHeight; ++y) {
        for (size_t x = 0; x < gb::kLcdWidth; ++x) {
            if (ids[y * gb::kLcdWidth + x] == 0xB0u) {
                ++found;
            }
        }
    }
    return found;
}

// only the game over banner puts font glyphs on screen once play has started
bool game_over_shown(const gb::Gameboy& gameboy) {
    for (uint16_t id : gameboy.framebuffer_tiles()) {
        if ((id & 0x100u) != 0) {
            continue;
        }
        const uint8_t tile = static_cast<uint8_t>(id);
        if (tile >= 0x21u && tile <= 0x5Fu) {
            return true;
        }
    }
    return false;
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

// entering play blanks the lcd for a few frames, so settle before reading the screen
constexpr uint32_t kEnterPlayFrames = 6;

// boots the rom and leaves it a few frames into the play state
void start_play(gb::Gameboy& gameboy, const std::vector<uint8_t>& rom) {
    REQUIRE(gameboy.load_rom(rom));
    run(gameboy, kBootFrames);
    press(gameboy, gb::Button::Start, 2);
    run(gameboy, kEnterPlayFrames);
}

// runs one frame, tapping a every tenth; the steady lift pins the bird at the ceiling
void step_flapping(gb::Gameboy& gameboy, uint32_t frame) {
    gameboy.set_button(gb::Button::A, frame % 10 == 0);
    gameboy.run_frame();
    gameboy.set_button(gb::Button::A, false);
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
    // short wait: the ground kills the bird about 25 frames into a silent run
    run(gameboy, 12);
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

TEST_CASE("bird_never_sinks_past_the_ground") {
    const std::vector<uint8_t> rom = read_flappy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    bool ended = false;
    for (uint32_t i = 0; i < 600; ++i) {
        gameboy.run_frame();
        const int y = bird_y(gameboy);
        REQUIRE(y != kNoBird);
        REQUIRE(y >= 0);
        REQUIRE(y + kBirdSizePx <= kGroundTopPx);
        ended = ended || game_over_shown(gameboy);
    }
    // the ceiling still clamps, but the ground ends the run instead of holding the bird up
    REQUIRE(ended);
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

TEST_CASE("pipes_scroll_left") {
    const std::vector<uint8_t> rom = read_flappy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    uint32_t frame = 0;
    for (; frame < 60; ++frame) {
        step_flapping(gameboy, frame);
    }

    const int first = leftmost_pipe_x(gameboy);
    REQUIRE(first != kNoPipe);
    // clear of the bird's own column, so no sprite pixel can hide a pipe pixel
    REQUIRE(first > kBirdScreenX + kBirdSizePx);

    int prev = first;
    for (uint32_t i = 0; i < 32; ++i, ++frame) {
        step_flapping(gameboy, frame);
        const int x = leftmost_pipe_x(gameboy);
        REQUIRE(x != kNoPipe);
        REQUIRE(x < prev);
        prev = x;
    }
    REQUIRE(first - prev >= 16);
}

TEST_CASE("ground_is_present_and_scrolling") {
    const std::vector<uint8_t> rom = read_flappy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    const size_t bottom_pixels = (gb::kLcdHeight - kGroundTopPx) * gb::kLcdWidth;
    REQUIRE(count_ground_pixels(gameboy) == bottom_pixels);

    // the streamer rewrites every column it scrolls past; the ground must survive that
    for (uint32_t frame = 0; frame < 90; ++frame) {
        step_flapping(gameboy, frame);
        REQUIRE(count_ground_pixels(gameboy) == bottom_pixels);
    }
}

TEST_CASE("falling_to_the_ground_ends_the_run") {
    const std::vector<uint8_t> rom = read_flappy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    run(gameboy, 120);
    REQUIRE(game_over_shown(gameboy));

    const int resting = bird_y(gameboy);
    REQUIRE(resting != kNoBird);
    REQUIRE(resting > kGroundTopPx - 2 * kBirdSizePx);
    run(gameboy, 60);
    REQUIRE(bird_y(gameboy) == resting);
}

TEST_CASE("hitting_a_pipe_ends_the_run") {
    const std::vector<uint8_t> rom = read_flappy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    int killed_at = kNoBird;
    for (uint32_t frame = 0; frame < 250; ++frame) {
        step_flapping(gameboy, frame);
        if (game_over_shown(gameboy)) {
            killed_at = bird_y(gameboy);
            break;
        }
    }
    REQUIRE(killed_at != kNoBird);
    // constant flapping pins the bird at the ceiling, so this is the pipe's upper arm
    REQUIRE(killed_at < kGroundTopPx / 2);
}

TEST_CASE("restart_gives_a_fresh_run") {
    const std::vector<uint8_t> rom = read_flappy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    run(gameboy, 120);
    REQUIRE(game_over_shown(gameboy));

    press(gameboy, gb::Button::Start, 2);
    // twice the usual settle: the old sprite position lingers while the lcd is off
    run(gameboy, 2 * kEnterPlayFrames);
    REQUIRE_FALSE(game_over_shown(gameboy));
    // scroll is back at zero, so the first pipe is still off the right edge
    REQUIRE(leftmost_pipe_x(gameboy) == kNoPipe);

    const int y = bird_y(gameboy);
    REQUIRE(y != kNoBird);
    REQUIRE(y + kBirdSizePx < kGroundTopPx);
    run(gameboy, 12);
    REQUIRE(bird_y(gameboy) > y);
}

// searched locally against this rom: a flap every 21 frames threads the first gap
// re-search after any rom change: the gap seed is div at world_init
constexpr std::array<uint32_t, 10> kSurvivingFlaps = {12, 33, 54, 75, 96, 117, 138, 159, 180, 201};

TEST_CASE("a_scripted_run_survives_the_first_pipe") {
    const std::vector<uint8_t> rom = read_flappy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    bool cleared = false;
    for (uint32_t frame = 0; frame < 220; ++frame) {
        const bool flap =
            std::find(kSurvivingFlaps.begin(), kSurvivingFlaps.end(), frame) != kSurvivingFlaps.end();
        gameboy.set_button(gb::Button::A, flap);
        gameboy.run_frame();
        gameboy.set_button(gb::Button::A, false);
        REQUIRE_FALSE(game_over_shown(gameboy));
        const int x = leftmost_pipe_x(gameboy);
        // the whole first pipe is behind the bird's column by now
        cleared = cleared || (x != kNoPipe && x + 20 < kBirdScreenX);
    }
    REQUIRE(cleared);
}

namespace {

constexpr uint32_t kScriptFrames = 220;

// one frame of the scripted run
void script_frame(gb::Gameboy& gameboy, uint32_t frame) {
    const bool flap =
        std::find(kSurvivingFlaps.begin(), kSurvivingFlaps.end(), frame) != kSurvivingFlaps.end();
    gameboy.set_button(gb::Button::A, flap);
    gameboy.run_frame();
    gameboy.set_button(gb::Button::A, false);
}

void run_script(gb::Gameboy& gameboy, uint32_t frames) {
    for (uint32_t frame = 0; frame < frames; ++frame) {
        script_frame(gameboy, frame);
    }
}

// stops flapping and waits for the banner
void run_until_over(gb::Gameboy& gameboy, uint32_t limit) {
    for (uint32_t i = 0; i < limit; ++i) {
        gameboy.run_frame();
        if (game_over_shown(gameboy)) {
            return;
        }
    }
    FAIL("the run never reached game over");
}

int16_t peak_audio(gb::Gameboy& gameboy, uint32_t frames) {
    std::array<int16_t, 8192> buffer{};
    int16_t peak = 0;
    for (uint32_t i = 0; i < frames; ++i) {
        gameboy.run_frame();
        const size_t got = gameboy.read_audio(buffer);
        for (size_t s = 0; s < got; ++s) {
            const int16_t mag = static_cast<int16_t>(buffer[s] < 0 ? -buffer[s] : buffer[s]);
            peak = std::max(peak, mag);
        }
    }
    return peak;
}

} // namespace

TEST_CASE("score_hud_counts_passed_pipes") {
    const std::vector<uint8_t> rom = read_flappy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    // no pipe has gone by yet, so the hud sits on a single zero
    REQUIRE(hud_shows_digit(gameboy, 0));
    REQUIRE_FALSE(hud_shows_digit(gameboy, 1));

    bool scored = false;
    for (uint32_t frame = 0; frame < kScriptFrames && !scored; ++frame) {
        script_frame(gameboy, frame);
        REQUIRE_FALSE(game_over_shown(gameboy));
        scored = !hud_shows_digit(gameboy, 0);
    }
    REQUIRE(scored);
    REQUIRE(hud_shows_digit(gameboy, 1));
}

TEST_CASE("best_score_lands_in_sram") {
    const std::vector<uint8_t> rom = read_flappy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    run_script(gameboy, kScriptFrames);
    run_until_over(gameboy, 300);

    const std::span<uint8_t> ram = gameboy.external_ram();
    REQUIRE(sram_has_magic(ram));
    REQUIRE(sram_best(ram) >= 1u);
}

TEST_CASE("best_score_survives_reload") {
    const std::vector<uint8_t> rom = read_flappy_rom();

    gb::Gameboy first;
    start_play(first, rom);
    run_script(first, kScriptFrames);
    run_until_over(first, 300);
    const std::vector<uint8_t> saved(first.external_ram().begin(), first.external_ram().end());
    const uint16_t best = sram_best(saved);
    REQUIRE(best >= 1u);

    gb::Gameboy second;
    REQUIRE(second.load_rom(rom));
    const std::span<uint8_t> ram = second.external_ram();
    REQUIRE(ram.size() == saved.size());
    std::copy(saved.begin(), saved.end(), ram.begin());

    // a scoreless run must read the saved best instead of re-initialising it
    run(second, kBootFrames);
    press(second, gb::Button::Start, 2);
    run(second, kEnterPlayFrames);
    run_until_over(second, 300);
    REQUIRE(sram_has_magic(second.external_ram()));
    REQUIRE(sram_best(second.external_ram()) == best);
}

TEST_CASE("game_over_shows_best") {
    const std::vector<uint8_t> rom = read_flappy_rom();

    gb::Gameboy quiet;
    start_play(quiet, rom);
    run_until_over(quiet, 300);
    REQUIRE(banner_has_glyph(quiet, font_tile('S')));
    REQUIRE(banner_has_glyph(quiet, font_tile('B')));
    // a scoreless first death: score and best both read zero
    REQUIRE(banner_has_glyph(quiet, font_tile('0')));
    REQUIRE_FALSE(banner_has_nonzero_digit(quiet));

    gb::Gameboy scored;
    start_play(scored, rom);
    run_script(scored, kScriptFrames);
    run_until_over(scored, 300);
    REQUIRE(banner_has_nonzero_digit(scored));
}

TEST_CASE("flap_makes_sound") {
    const std::vector<uint8_t> rom = read_flappy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    std::array<int16_t, 8192> drain{};
    while (gameboy.read_audio(drain) != 0) {
    }

    // measured locally: silence is exactly 0 and the flap peaks near 3800
    const int16_t quiet = peak_audio(gameboy, 10);
    REQUIRE(quiet < 64);
    gameboy.set_button(gb::Button::A, true);
    gameboy.run_frame();
    gameboy.set_button(gb::Button::A, false);
    REQUIRE(peak_audio(gameboy, 10) > 1000);
}

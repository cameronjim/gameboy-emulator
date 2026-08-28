#include "cartridge.hpp"
#include "gameboy.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
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

// the bird is any of its three flap frames
constexpr uint8_t kBirdTileFirst = 0xE0;
constexpr uint8_t kBirdTileLast = 0xE2;
constexpr uint8_t kDigitTileId = 0xD0;
constexpr uint8_t kGroundTileId = 0xB0;

// the popup draws from an inverted copy of the font parked at 0x60
constexpr uint8_t kInvFontFirstTile = 0x60;
constexpr uint8_t kInvFontLastTile = 0x9F;

// screen rows of the hover screen's three text lines
constexpr size_t kTitleRow = 6;
constexpr size_t kPromptRow = 10;
constexpr size_t kBestRow = kPromptRow + 2;

// the popup band covers screen rows 5..11 of 18
constexpr size_t kPopupTopPx = 40;
constexpr size_t kPopupEndPx = 96;

// input is ignored for this many frames after a crash
constexpr uint32_t kLockoutFrames = 20;

bool is_bird_pixel(uint16_t id) {
    const uint8_t tile = static_cast<uint8_t>(id);
    return (id & 0x100u) != 0 && tile >= kBirdTileFirst && tile <= kBirdTileLast;
}

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
        if (is_bird_pixel(ids[i]) && fb[i] != 0) {
            return static_cast<int>(i / gb::kLcdWidth);
        }
    }
    return kNoBird;
}

size_t count_bird_pixels(const gb::Gameboy& gameboy) {
    const std::span<const uint16_t> ids = gameboy.framebuffer_tiles();
    const std::span<const uint8_t> fb = gameboy.framebuffer();
    size_t lit = 0;
    for (size_t i = 0; i < ids.size(); ++i) {
        if (is_bird_pixel(ids[i]) && fb[i] != 0) {
            ++lit;
        }
    }
    return lit;
}

bool hud_shows_digit(const gb::Gameboy& gameboy, uint8_t digit) {
    return count_sprite_pixels_of(gameboy, static_cast<uint8_t>(kDigitTileId + digit)) > 0;
}

// one drawn hud digit: the glyph tile plus the extent of its sprite pixels
struct DigitRun {
    uint8_t digit;
    int x0;
    int x1;
    int y0;
    int y1;
    size_t pixels;
};

// the hud sits in the top rows; each digit badge leaves a blank column beside the next
std::vector<DigitRun> hud_digits(const gb::Gameboy& gameboy) {
    const std::span<const uint16_t> ids = gameboy.framebuffer_tiles();
    const std::span<const uint8_t> fb = gameboy.framebuffer();
    constexpr size_t kHudBandRows = 40;
    std::vector<DigitRun> runs;
    bool open = false;
    for (size_t x = 0; x < gb::kLcdWidth; ++x) {
        int digit = -1;
        int y0 = 0;
        int y1 = 0;
        size_t pixels = 0;
        for (size_t y = 0; y < kHudBandRows; ++y) {
            const size_t i = y * gb::kLcdWidth + x;
            const uint8_t tile = static_cast<uint8_t>(ids[i]);
            if ((ids[i] & 0x100u) == 0 || tile < kDigitTileId || tile > kDigitTileId + 9 || fb[i] == 0) {
                continue;
            }
            if (pixels == 0) {
                digit = tile - kDigitTileId;
                y0 = static_cast<int>(y);
            }
            y1 = static_cast<int>(y);
            ++pixels;
        }
        if (digit < 0) {
            open = false;
            continue;
        }
        if (!open) {
            runs.push_back(
                DigitRun{static_cast<uint8_t>(digit), static_cast<int>(x), static_cast<int>(x), y0, y1, 0});
            open = true;
        }
        DigitRun& run = runs.back();
        run.x1 = static_cast<int>(x);
        run.y0 = std::min(run.y0, y0);
        run.y1 = std::max(run.y1, y1);
        run.pixels += pixels;
    }
    return runs;
}

int hud_score(const gb::Gameboy& gameboy) {
    int value = 0;
    for (const DigitRun& run : hud_digits(gameboy)) {
        value = value * 10 + run.digit;
    }
    return value;
}

// gbdk's ibm font lands ascii 0x20-0x7f on tiles 0x00-0x5f
constexpr uint8_t font_tile(char c) {
    return static_cast<uint8_t>(c - 0x20);
}

// the popup writes the inverted copy of the same glyph
constexpr uint8_t popup_tile(char c) {
    return static_cast<uint8_t>(kInvFontFirstTile + font_tile(c));
}

bool bg_has_tile(const gb::Gameboy& gameboy, uint8_t tile) {
    for (uint16_t id : gameboy.framebuffer_tiles()) {
        if ((id & 0x100u) == 0 && static_cast<uint8_t>(id) == tile) {
            return true;
        }
    }
    return false;
}

bool row_has_tile(const gb::Gameboy& gameboy, size_t row, uint8_t tile) {
    const std::span<const uint16_t> ids = gameboy.framebuffer_tiles();
    for (size_t y = row * 8; y < row * 8 + 8; ++y) {
        for (size_t x = 0; x < gb::kLcdWidth; ++x) {
            const uint16_t id = ids[y * gb::kLcdWidth + x];
            if ((id & 0x100u) == 0 && static_cast<uint8_t>(id) == tile) {
                return true;
            }
        }
    }
    return false;
}

bool row_has_nonzero_digit(const gb::Gameboy& gameboy, size_t row) {
    for (char c = '1'; c <= '9'; ++c) {
        if (row_has_tile(gameboy, row, font_tile(c))) {
            return true;
        }
    }
    return false;
}

// inverted font cells only ever reach the screen through the game over popup
bool popup_shown(const gb::Gameboy& gameboy) {
    for (uint16_t id : gameboy.framebuffer_tiles()) {
        if ((id & 0x100u) != 0) {
            continue;
        }
        const uint8_t tile = static_cast<uint8_t>(id);
        if (tile >= kInvFontFirstTile && tile <= kInvFontLastTile) {
            return true;
        }
    }
    return false;
}

// plain font cells only ever reach the screen through the hover screen
bool title_shown(const gb::Gameboy& gameboy) {
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

bool popup_has_nonzero_digit(const gb::Gameboy& gameboy) {
    for (char c = '1'; c <= '9'; ++c) {
        if (bg_has_tile(gameboy, popup_tile(c))) {
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

bool is_ground_tile(uint16_t id) {
    return (id & 0x100u) == 0 && static_cast<uint8_t>(id) == kGroundTileId;
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

// the bird is parked offscreen the frame the run ends
bool alive(const gb::Gameboy& gameboy) {
    return bird_y(gameboy) != kNoBird;
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

TEST_CASE("title_shows_bobbing_bird") {
    const std::vector<uint8_t> rom = read_flappy_rom();

    gb::Gameboy gameboy;
    REQUIRE(gameboy.load_rom(rom));
    run(gameboy, kBootFrames);
    REQUIRE(count_bird_pixels(gameboy) > 0u);

    int lowest = 0;
    int highest = gb::kLcdHeight;
    for (uint32_t i = 0; i < 120; ++i) {
        gameboy.run_frame();
        const int y = bird_y(gameboy);
        REQUIRE(y != kNoBird);
        lowest = std::max(lowest, y);
        highest = std::min(highest, y);
        // the title never falls into a run, so the popup must never appear
        REQUIRE_FALSE(popup_shown(gameboy));
    }
    // a bob, not a fall: the bird oscillates inside a narrow band
    REQUIRE(lowest - highest >= 2);
    REQUIRE(lowest - highest <= 16);
}

TEST_CASE("hover_shows_best") {
    const std::vector<uint8_t> rom = read_flappy_rom();

    gb::Gameboy gameboy;
    REQUIRE(gameboy.load_rom(rom));
    run(gameboy, kBootFrames);
    // the prompt names the flap key and the line under it reads best 0 on fresh sram
    REQUIRE(row_has_tile(gameboy, kPromptRow, font_tile('F')));
    REQUIRE(row_has_tile(gameboy, kBestRow, font_tile('B')));
    REQUIRE(row_has_tile(gameboy, kBestRow, font_tile('0')));
    REQUIRE_FALSE(row_has_nonzero_digit(gameboy, kBestRow));
}

TEST_CASE("run_starts_on_first_flap") {
    const std::vector<uint8_t> rom = read_flappy_rom();

    gb::Gameboy gameboy;
    REQUIRE(gameboy.load_rom(rom));
    run(gameboy, kBootFrames);
    const int hovering = bird_y(gameboy);
    REQUIRE(hovering != kNoBird);

    press(gameboy, gb::Button::A, 2);
    std::vector<int> ys;
    for (uint32_t i = 0; i < 40; ++i) {
        gameboy.run_frame();
        const int y = bird_y(gameboy);
        if (y != kNoBird) {
            ys.push_back(y);
        }
    }
    REQUIRE(ys.size() > 20u);
    const size_t apex = static_cast<size_t>(std::min_element(ys.begin(), ys.end()) - ys.begin());
    // the press is the first flap: the bird climbs clear of its hover height before it sinks
    REQUIRE(ys[apex] < hovering - 8);
    for (size_t i = 1; i <= apex; ++i) {
        REQUIRE(ys[i] <= ys[i - 1]);
    }
    REQUIRE(ys.back() > ys[apex]);

    // and the world is running: the first pipe scrolls in
    bool pipe = false;
    for (uint32_t frame = 0; frame < 260 && !pipe; ++frame) {
        step_flapping(gameboy, frame);
        pipe = leftmost_pipe_x(gameboy) != kNoPipe;
    }
    REQUIRE(pipe);
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
    // the start press flaps, so gravity has to undo that climb first
    int lowest = before;
    for (uint32_t i = 0; i < 45; ++i) {
        gameboy.run_frame();
        const int y = bird_y(gameboy);
        if (y != kNoBird) {
            lowest = std::max(lowest, y);
        }
    }
    REQUIRE(lowest > before);
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
    for (uint32_t i = 0; i < 600 && !ended; ++i) {
        gameboy.run_frame();
        const int y = bird_y(gameboy);
        // the bird is parked offscreen the frame the run ends
        if (y == kNoBird) {
            ended = true;
            break;
        }
        REQUIRE(y >= 0);
        REQUIRE(y + kBirdSizePx <= kGroundTopPx);
    }
    // the ceiling still clamps, but the ground ends the run instead of holding the bird up
    REQUIRE(ended);
    run(gameboy, 8);
    REQUIRE(popup_shown(gameboy));
}

TEST_CASE("holding_a_does_not_autofire") {
    const std::vector<uint8_t> rom = read_flappy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    std::vector<int> ys;
    gameboy.set_button(gb::Button::A, true);
    for (uint32_t i = 0; i < 60; ++i) {
        gameboy.run_frame();
        const int y = bird_y(gameboy);
        // the sample run stops at the crash, where the bird leaves the screen
        if (y == kNoBird) {
            break;
        }
        ys.push_back(y);
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

    // the bird vanishes the frame it dies, so keep the last height it was drawn at
    int resting = kNoBird;
    uint32_t frames = 0;
    for (; frames < 200; ++frames) {
        gameboy.run_frame();
        const int y = bird_y(gameboy);
        if (y == kNoBird) {
            break;
        }
        resting = y;
    }
    REQUIRE(frames < 200);
    REQUIRE(resting != kNoBird);
    REQUIRE(resting > kGroundTopPx - 2 * kBirdSizePx);

    // the round is frozen: the bird stays parked and the popup takes the screen
    run(gameboy, 60);
    REQUIRE(popup_shown(gameboy));
    REQUIRE(bird_y(gameboy) == kNoBird);
}

TEST_CASE("hitting_a_pipe_ends_the_run") {
    const std::vector<uint8_t> rom = read_flappy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    int killed_at = kNoBird;
    int last = kNoBird;
    for (uint32_t frame = 0; frame < 250; ++frame) {
        step_flapping(gameboy, frame);
        const int y = bird_y(gameboy);
        if (y == kNoBird) {
            killed_at = last;
            break;
        }
        last = y;
    }
    REQUIRE(killed_at != kNoBird);
    // constant flapping pins the bird at the ceiling, so this is the pipe's upper arm
    REQUIRE(killed_at < kGroundTopPx / 2);
}

// searched locally against this rom with a lookahead autopilot; frame 0 is the start press itself
// re-search after any rom change: the gap seed is div at world_init
constexpr std::array<uint32_t, 10> kSurvivingFlaps = {0, 32, 38, 44, 50, 81, 111, 118, 153, 193};

// the same search carried out to score 14, for the hud and difficulty tests
constexpr std::array<uint32_t, 53> kLongScript = {
    0,   32,  38,  44,   50,   81,   111,  118,  153,  193,  222,  252,  292,  311,  346,  361,  392, 405,
    442, 457, 486, 500,  537,  560,  592,  600,  635,  674,  693,  729,  763,  792,  823,  828,  862, 892,
    922, 952, 983, 1013, 1033, 1062, 1093, 1124, 1140, 1177, 1187, 1193, 1221, 1252, 1286, 1294, 1330};
// the long script clears its fourteenth pipe at this frame and stops flapping soon after
constexpr uint32_t kLongScriptFrames = 1360;

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
        REQUIRE(alive(gameboy));
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

void long_script_frame(gb::Gameboy& gameboy, uint32_t frame) {
    const bool flap = std::find(kLongScript.begin(), kLongScript.end(), frame) != kLongScript.end();
    gameboy.set_button(gb::Button::A, flap);
    gameboy.run_frame();
    gameboy.set_button(gb::Button::A, false);
}

// waits for the crash, then for the staged popup to finish drawing
void run_until_over(gb::Gameboy& gameboy, uint32_t limit) {
    for (uint32_t i = 0; i < limit; ++i) {
        gameboy.run_frame();
        if (popup_shown(gameboy)) {
            run(gameboy, 6);
            return;
        }
    }
    FAIL("the run never reached game over");
}

// the popup ignores input for a lockout, so wait it out before the dismissing press
void dismiss(gb::Gameboy& gameboy, gb::Button button) {
    run(gameboy, kLockoutFrames + 4);
    press(gameboy, button, 2);
    // twice the usual settle: the lcd is off while the hover screen is redrawn
    run(gameboy, 2 * kEnterPlayFrames);
}

// peak to peak, not peak: a triggered channel at volume zero still sits at its dac floor
int32_t audio_swing(gb::Gameboy& gameboy, uint32_t frames) {
    std::array<int16_t, 8192> buffer{};
    int16_t high = std::numeric_limits<int16_t>::min();
    int16_t low = std::numeric_limits<int16_t>::max();
    for (uint32_t i = 0; i < frames; ++i) {
        gameboy.run_frame();
        const size_t got = gameboy.read_audio(buffer);
        for (size_t s = 0; s < got; ++s) {
            high = std::max(high, buffer[s]);
            low = std::min(low, buffer[s]);
        }
    }
    return high < low ? 0 : static_cast<int32_t>(high) - static_cast<int32_t>(low);
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
        REQUIRE(alive(gameboy));
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
    REQUIRE(bg_has_tile(quiet, popup_tile('S')));
    REQUIRE(bg_has_tile(quiet, popup_tile('B')));
    // a scoreless first death: score and best both read zero
    REQUIRE(bg_has_tile(quiet, popup_tile('0')));
    REQUIRE_FALSE(popup_has_nonzero_digit(quiet));

    gb::Gameboy scored;
    start_play(scored, rom);
    run_script(scored, kScriptFrames);
    run_until_over(scored, 300);
    REQUIRE(popup_has_nonzero_digit(scored));
}

TEST_CASE("game_over_popup_is_centered_and_solid") {
    const std::vector<uint8_t> rom = read_flappy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    run_script(gameboy, kScriptFrames);
    run_until_over(gameboy, 300);

    const std::span<const uint16_t> ids = gameboy.framebuffer_tiles();
    for (size_t y = kPopupTopPx; y < kPopupEndPx; ++y) {
        for (size_t x = 0; x < gb::kLcdWidth; ++x) {
            const uint16_t id = ids[y * gb::kLcdWidth + x];
            // the whole band is popup fill or an inverted glyph: no world, no border, one colour
            REQUIRE_FALSE(is_pipe_tile(id));
            REQUIRE_FALSE(is_ground_tile(id));
            REQUIRE(static_cast<uint8_t>(id) >= kInvFontFirstTile);
            REQUIRE(static_cast<uint8_t>(id) <= kInvFontLastTile);
        }
    }

    // the frozen world still shows above and below the band
    bool above = false;
    bool below = false;
    for (size_t y = 0; y < gb::kLcdHeight; ++y) {
        for (size_t x = 0; x < gb::kLcdWidth; ++x) {
            const uint16_t id = ids[y * gb::kLcdWidth + x];
            if (!is_pipe_tile(id) && !is_ground_tile(id)) {
                continue;
            }
            above = above || y < kPopupTopPx;
            below = below || y >= kPopupEndPx;
        }
    }
    REQUIRE(above);
    REQUIRE(below);

    for (char c : std::string("GAMEOVRSCBPNYK")) {
        REQUIRE(bg_has_tile(gameboy, popup_tile(c)));
    }
    // the bird may have died right where the popup sits, so it is parked offscreen
    REQUIRE(count_bird_pixels(gameboy) == 0u);
}

TEST_CASE("an_early_flap_does_not_dismiss_the_popup") {
    const std::vector<uint8_t> rom = read_flappy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    uint32_t frames = 0;
    for (; frames < 300 && alive(gameboy); ++frames) {
        gameboy.run_frame();
    }
    REQUIRE(frames < 300);

    run(gameboy, 5);
    press(gameboy, gb::Button::A, 2);
    run(gameboy, 2 * kEnterPlayFrames);
    REQUIRE(popup_shown(gameboy));
    REQUIRE_FALSE(title_shown(gameboy));
}

TEST_CASE("any_button_returns_to_hover") {
    const std::vector<uint8_t> rom = read_flappy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    run_until_over(gameboy, 300);
    REQUIRE(popup_shown(gameboy));

    // b is neither the start nor the flap key, and it still clears the popup
    dismiss(gameboy, gb::Button::B);
    REQUIRE_FALSE(popup_shown(gameboy));
    REQUIRE(row_has_tile(gameboy, kTitleRow, font_tile('F')));
    REQUIRE(row_has_tile(gameboy, kTitleRow, font_tile('Y')));
    REQUIRE(leftmost_pipe_x(gameboy) == kNoPipe);

    int lowest = 0;
    int highest = gb::kLcdHeight;
    for (uint32_t i = 0; i < 80; ++i) {
        gameboy.run_frame();
        const int y = bird_y(gameboy);
        REQUIRE(y != kNoBird);
        lowest = std::max(lowest, y);
        highest = std::min(highest, y);
    }
    REQUIRE(lowest - highest >= 2);
    REQUIRE(lowest - highest <= 16);

    // one fresh a press starts the next run
    press(gameboy, gb::Button::A, 2);
    run(gameboy, 2 * kEnterPlayFrames);
    REQUIRE_FALSE(title_shown(gameboy));
    bool pipe = false;
    for (uint32_t frame = 0; frame < 300 && !pipe; ++frame) {
        step_flapping(gameboy, frame);
        pipe = leftmost_pipe_x(gameboy) != kNoPipe;
    }
    REQUIRE(pipe);
}

TEST_CASE("dismiss_press_does_not_start_a_run") {
    const std::vector<uint8_t> rom = read_flappy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    run_until_over(gameboy, 300);
    dismiss(gameboy, gb::Button::B);
    REQUIRE(title_shown(gameboy));

    for (uint32_t i = 0; i < 60; ++i) {
        gameboy.run_frame();
        // the press that cleared the popup is spent: the hover screen just keeps hovering
        REQUIRE(title_shown(gameboy));
        REQUIRE(leftmost_pipe_x(gameboy) == kNoPipe);
        REQUIRE(alive(gameboy));
    }
}

TEST_CASE("restart_gives_a_fresh_run") {
    const std::vector<uint8_t> rom = read_flappy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    run_until_over(gameboy, 300);
    REQUIRE(popup_shown(gameboy));

    dismiss(gameboy, gb::Button::Start);
    REQUIRE(title_shown(gameboy));
    // best carries over into the hover screen
    REQUIRE(row_has_tile(gameboy, kBestRow, font_tile('B')));

    press(gameboy, gb::Button::A, 2);
    run(gameboy, 2 * kEnterPlayFrames);
    REQUIRE_FALSE(title_shown(gameboy));
    REQUIRE_FALSE(popup_shown(gameboy));
    // scroll is back at zero, so the first pipe is still off the right edge
    REQUIRE(leftmost_pipe_x(gameboy) == kNoPipe);

    const int y = bird_y(gameboy);
    REQUIRE(y != kNoBird);
    REQUIRE(y + kBirdSizePx < kGroundTopPx);
    // the starting press flaps too, so the bird climbs before gravity takes it back down
    int lowest = y;
    for (uint32_t i = 0; i < 40; ++i) {
        gameboy.run_frame();
        const int cur = bird_y(gameboy);
        if (cur == kNoBird) {
            break;
        }
        lowest = std::max(lowest, cur);
    }
    REQUIRE(lowest > y);
}

TEST_CASE("hover_shows_a_new_best") {
    const std::vector<uint8_t> rom = read_flappy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    run_script(gameboy, kScriptFrames);
    run_until_over(gameboy, 300);
    dismiss(gameboy, gb::Button::B);
    REQUIRE(title_shown(gameboy));
    REQUIRE(row_has_nonzero_digit(gameboy, kBestRow));
}

TEST_CASE("flap_makes_sound") {
    const std::vector<uint8_t> rom = read_flappy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    // the start press is itself a flap, so let its envelope run out first
    run(gameboy, 26);
    std::array<int16_t, 8192> drain{};
    while (gameboy.read_audio(drain) != 0) {
    }

    // measured locally: a decayed channel holds one dc level and the flap swings about 7600
    const int32_t quiet = audio_swing(gameboy, 6);
    REQUIRE(quiet < 512);
    gameboy.set_button(gb::Button::A, true);
    gameboy.run_frame();
    gameboy.set_button(gb::Button::A, false);
    REQUIRE(audio_swing(gameboy, 10) > 1000);
}

TEST_CASE("two_digit_score_renders_fully") {
    const std::vector<uint8_t> rom = read_flappy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    bool two = false;
    for (uint32_t frame = 0; frame < kLongScriptFrames && !two; ++frame) {
        long_script_frame(gameboy, frame);
        REQUIRE(alive(gameboy));
        two = hud_score(gameboy) >= 10;
    }
    REQUIRE(two);

    const std::vector<DigitRun> digits = hud_digits(gameboy);
    REQUIRE(digits.size() == 2u);
    for (const DigitRun& run : digits) {
        // a badge is a full 8 row tile seven columns wide, and none of it is off screen
        REQUIRE(run.y1 - run.y0 == 7);
        REQUIRE(run.x1 - run.x0 == 6);
        REQUIRE(run.x0 > 0);
        REQUIRE(run.x1 < static_cast<int>(gb::kLcdWidth) - 1);
        REQUIRE(run.pixels >= 40u);
    }
    REQUIRE(digits[1].x0 - digits[0].x0 == 8);
}

namespace {

// px per frame over the longest stretch a pipe stays fully on screen inside a score band
double scroll_rate(const std::vector<int>& xs, const std::vector<int>& scores, int lo, int hi) {
    int best_frames = 0;
    int best_px = 0;
    for (size_t i = 1; i < xs.size(); ++i) {
        const auto in_band = [&](size_t k) { return scores[k] >= lo && scores[k] <= hi; };
        if (!in_band(i) || xs[i] < 2 || xs[i - 1] < 2) {
            continue;
        }
        size_t j = i;
        int px = 0;
        while (j < xs.size() && xs[j] >= 2 && xs[j] <= xs[j - 1] && in_band(j)) {
            px += xs[j - 1] - xs[j];
            ++j;
        }
        if (static_cast<int>(j - i) > best_frames) {
            best_frames = static_cast<int>(j - i);
            best_px = px;
        }
        i = j;
    }
    REQUIRE(best_frames > 30);
    return static_cast<double>(best_px) / best_frames;
}

} // namespace

TEST_CASE("difficulty_ramps") {
    const std::vector<uint8_t> rom = read_flappy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    std::vector<int> xs;
    std::vector<int> scores;
    for (uint32_t frame = 0; frame < kLongScriptFrames; ++frame) {
        long_script_frame(gameboy, frame);
        REQUIRE(alive(gameboy));
        xs.push_back(leftmost_pipe_x(gameboy));
        scores.push_back(hud_score(gameboy));
    }
    REQUIRE(scores.back() >= 11);

    const double early = scroll_rate(xs, scores, 0, 4);
    const double late = scroll_rate(xs, scores, 11, 99);
    // the table steps 1.00 to 1.25 px per frame once ten pipes are behind the bird
    REQUIRE(early > 0.9);
    REQUIRE(early < 1.1);
    REQUIRE(late > early + 0.15);
    REQUIRE(late < 1.4);
}

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

// the tile contract: grass and tree on the bg, chick and digits as sprites
constexpr uint8_t kGrassTileId = 0xA0;
constexpr uint8_t kTreeTileId = 0xA1;
constexpr uint8_t kChickTileId = 0xE0;
constexpr uint8_t kChickHopTileId = 0xE1;
constexpr uint8_t kDigitTileId = 0xD0;

// the grid: 10 columns of 16 px, the chick's cell inset 4 px inside its own
constexpr int kGridCols = 10;
constexpr int kCellPx = 16;
constexpr int kCellInset = 4;

// a hop slides 16 px over 8 frames, so ten frames always settles one
constexpr uint32_t kHopFrames = 10;
constexpr uint32_t kSettleFrames = 12;

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

constexpr int kNoTile = -1;

// the first bg tile inside an 8x8 screen cell; sprites over the cell are stepped past
int bg_cell(const gb::Gameboy& gameboy, int cx, int cy) {
    const std::span<const uint16_t> ids = gameboy.framebuffer_tiles();
    if (cy < 0 || cy > 17 || cx < 0 || cx > 19) {
        return kNoTile;
    }
    for (int y = cy * 8; y < cy * 8 + 8; ++y) {
        for (int x = cx * 8; x < cx * 8 + 8; ++x) {
            const uint16_t id = ids[static_cast<size_t>(y) * gb::kLcdWidth + static_cast<size_t>(x)];
            if ((id & 0x100u) == 0) {
                return static_cast<uint8_t>(id);
            }
        }
    }
    return kNoTile;
}

struct Chick {
    int x;
    int y;
    bool found;
    bool hopping;
};

// top-left corner of the chick's lit sprite pixels
Chick chick_at(const gb::Gameboy& gameboy) {
    const std::span<const uint16_t> ids = gameboy.framebuffer_tiles();
    const std::span<const uint8_t> fb = gameboy.framebuffer();
    Chick c{0, 0, false, false};
    for (size_t i = 0; i < ids.size(); ++i) {
        const uint8_t tile = static_cast<uint8_t>(ids[i]);
        if ((ids[i] & 0x100u) == 0 || fb[i] == 0) {
            continue;
        }
        if (tile != kChickTileId && tile != kChickHopTileId) {
            continue;
        }
        const int x = static_cast<int>(i % gb::kLcdWidth);
        const int y = static_cast<int>(i / gb::kLcdWidth);
        if (!c.found || x < c.x) {
            c.x = x;
        }
        if (!c.found || y < c.y) {
            c.y = y;
        }
        c.hopping = c.hopping || tile == kChickHopTileId;
        c.found = true;
    }
    return c;
}

int chick_col(const Chick& c) {
    return (c.x - kCellInset) / kCellPx;
}

// the bg tile of the grid cell k lanes ahead of the chick, in grid column gcol
int cell_tile(const gb::Gameboy& gameboy, const Chick& c, int k, int gcol) {
    const int row = (c.y - kCellInset) / 8 - 2 * k;
    for (int dr = 0; dr < 2; ++dr) {
        for (int dc = 0; dc < 2; ++dc) {
            const int v = bg_cell(gameboy, gcol * 2 + dc, row + dr);
            if (v != kNoTile) {
                return v;
            }
        }
    }
    return kNoTile;
}

bool cell_blocked(const gb::Gameboy& gameboy, const Chick& c, int k, int gcol) {
    return cell_tile(gameboy, c, k, gcol) == kTreeTileId;
}

enum Move { kLeft = 0, kRight = 1, kUp = 2, kNoMove = -1 };

// the autopilot: breadth first over the visible lanes, then the first step toward the furthest
int plan_move(const gb::Gameboy& gameboy, const Chick& c, int lanes) {
    constexpr int kUnseen = -2;
    constexpr int kStart = -1;
    std::vector<int> first(static_cast<size_t>(lanes * kGridCols), kUnseen);
    std::vector<std::pair<int, int>> queue;
    const int gcol = chick_col(c);
    if (gcol < 0 || gcol >= kGridCols) {
        return kNoMove;
    }
    first[static_cast<size_t>(gcol)] = kStart;
    queue.push_back({0, gcol});

    int best = kNoMove;
    int best_lane = -1;
    for (size_t i = 0; i < queue.size(); ++i) {
        const int k = queue[i].first;
        const int col = queue[i].second;
        const int came = first[static_cast<size_t>(k * kGridCols + col)];
        if (k > best_lane) {
            best_lane = k;
            best = came;
        }
        constexpr int kStepLane[3] = {0, 0, 1};
        constexpr int kStepCol[3] = {-1, 1, 0};
        for (int m = 0; m < 3; ++m) {
            const int nk = k + kStepLane[m];
            const int nc = col + kStepCol[m];
            if (nk >= lanes || nc < 0 || nc >= kGridCols) {
                continue;
            }
            const size_t at = static_cast<size_t>(nk * kGridCols + nc);
            if (first[at] != kUnseen || cell_blocked(gameboy, c, nk, nc)) {
                continue;
            }
            first[at] = (came == kStart) ? m : came;
            queue.push_back({nk, nc});
        }
    }
    return best_lane > 0 ? best : kNoMove;
}

// one drawn hud digit: the glyph tile plus the extent of its sprite pixels
struct DigitRun {
    uint8_t digit;
    int x0;
    int x1;
    size_t pixels;
};

std::vector<DigitRun> hud_digits(const gb::Gameboy& gameboy) {
    const std::span<const uint16_t> ids = gameboy.framebuffer_tiles();
    const std::span<const uint8_t> fb = gameboy.framebuffer();
    constexpr size_t kHudBandRows = 40;
    std::vector<DigitRun> runs;
    bool open = false;
    for (size_t x = 0; x < gb::kLcdWidth; ++x) {
        int digit = -1;
        size_t pixels = 0;
        for (size_t y = 0; y < kHudBandRows; ++y) {
            const size_t i = y * gb::kLcdWidth + x;
            const uint8_t tile = static_cast<uint8_t>(ids[i]);
            if ((ids[i] & 0x100u) == 0 || tile < kDigitTileId || tile > kDigitTileId + 9 || fb[i] == 0) {
                continue;
            }
            if (pixels == 0) {
                digit = tile - kDigitTileId;
            }
            ++pixels;
        }
        if (digit < 0) {
            open = false;
            continue;
        }
        if (!open) {
            runs.push_back(
                DigitRun{static_cast<uint8_t>(digit), static_cast<int>(x), static_cast<int>(x), 0});
            open = true;
        }
        runs.back().x1 = static_cast<int>(x);
        runs.back().pixels += pixels;
    }
    return runs;
}

int hud_score(const gb::Gameboy& gameboy) {
    int value = 0;
    const std::vector<DigitRun> runs = hud_digits(gameboy);
    if (runs.empty()) {
        return -1;
    }
    for (const DigitRun& run : runs) {
        value = value * 10 + run.digit;
    }
    return value;
}

void run(gb::Gameboy& gameboy, uint32_t frames) {
    for (uint32_t i = 0; i < frames; ++i) {
        gameboy.run_frame();
    }
}

// a two frame tap, then long enough for the whole hop to land
void tap(gb::Gameboy& gameboy, gb::Button button) {
    gameboy.set_button(button, true);
    gameboy.run_frame();
    gameboy.run_frame();
    gameboy.set_button(button, false);
    run(gameboy, kSettleFrames);
}

// entering play blanks the lcd for a few frames, so settle before reading the screen
constexpr uint32_t kEnterPlayFrames = 8;

void start_play(gb::Gameboy& gameboy, const std::vector<uint8_t>& rom) {
    REQUIRE(gameboy.load_rom(rom));
    run(gameboy, kBootFrames);
    gameboy.set_button(gb::Button::Start, true);
    gameboy.run_frame();
    gameboy.run_frame();
    gameboy.set_button(gb::Button::Start, false);
    run(gameboy, kEnterPlayFrames);
}

gb::Button button_for(int move) {
    if (move == kLeft) {
        return gb::Button::Left;
    }
    if (move == kRight) {
        return gb::Button::Right;
    }
    return gb::Button::Up;
}

// steers up the guaranteed path until the hud reaches target or the steps run out
int autopilot(gb::Gameboy& gameboy, int target, int steps) {
    for (int i = 0; i < steps; ++i) {
        const Chick c = chick_at(gameboy);
        REQUIRE(c.found);
        const int move = plan_move(gameboy, c, 7);
        if (move == kNoMove) {
            break;
        }
        tap(gameboy, button_for(move));
        if (hud_score(gameboy) >= target) {
            break;
        }
    }
    return hud_score(gameboy);
}

// every cell of the play area, minus the ones a sprite fully covers
bool play_area_is_all_terrain(const gb::Gameboy& gameboy) {
    for (int cy = 0; cy < 18; ++cy) {
        for (int cx = 0; cx < 20; ++cx) {
            const int tile = bg_cell(gameboy, cx, cy);
            if (tile != kNoTile && tile != kGrassTileId && tile != kTreeTileId) {
                return false;
            }
        }
    }
    return true;
}

std::vector<int> play_grid(const gb::Gameboy& gameboy) {
    std::vector<int> grid;
    for (int cy = 0; cy < 18; ++cy) {
        for (int cx = 0; cx < 20; ++cx) {
            grid.push_back(bg_cell(gameboy, cx, cy));
        }
    }
    return grid;
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

TEST_CASE("crossy_hover_shows_chick") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    REQUIRE(gameboy.load_rom(rom));
    run(gameboy, kBootFrames);
    const Chick hover = chick_at(gameboy);
    REQUIRE(hover.found);
    REQUIRE_FALSE(hover.hopping);

    gameboy.set_button(gb::Button::Start, true);
    gameboy.run_frame();
    gameboy.run_frame();
    gameboy.set_button(gb::Button::Start, false);
    run(gameboy, kEnterPlayFrames);

    const Chick playing = chick_at(gameboy);
    REQUIRE(playing.found);
    REQUIRE(chick_col(playing) == 4);
    // the run opens on plain grass, so the terrain is up before the first hop
    REQUIRE(play_area_is_all_terrain(gameboy));
}

TEST_CASE("crossy_hop_moves_one_cell") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    const Chick home = chick_at(gameboy);
    REQUIRE(home.found);

    gameboy.set_button(gb::Button::Right, true);
    gameboy.run_frame();
    gameboy.run_frame();
    gameboy.set_button(gb::Button::Right, false);
    run(gameboy, kHopFrames);
    const Chick right = chick_at(gameboy);
    REQUIRE(right.x == home.x + kCellPx);
    REQUIRE(right.y == home.y);

    // the hop has landed, so more frames must not move the chick any further
    run(gameboy, kSettleFrames);
    REQUIRE(chick_at(gameboy).x == right.x);

    tap(gameboy, gb::Button::Left);
    REQUIRE(chick_at(gameboy).x == home.x);

    // a forward hop slides the world down two tile rows and leaves the chick put
    const std::vector<int> before = play_grid(gameboy);
    tap(gameboy, gb::Button::Up);
    const Chick ahead = chick_at(gameboy);
    REQUIRE(ahead.x == home.x);
    REQUIRE(ahead.y == home.y);

    const std::vector<int> after = play_grid(gameboy);
    size_t compared = 0;
    for (int cy = 2; cy < 18; ++cy) {
        for (int cx = 0; cx < 20; ++cx) {
            const int was = before[static_cast<size_t>((cy - 2) * 20 + cx)];
            const int now = after[static_cast<size_t>(cy * 20 + cx)];
            if (was == kNoTile || now == kNoTile) {
                continue;
            }
            REQUIRE(now == was);
            ++compared;
        }
    }
    REQUIRE(compared > 300u);
}

TEST_CASE("crossy_tree_blocks_the_hop") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    bool tested = false;
    for (int step = 0; step < 40 && !tested; ++step) {
        const Chick c = chick_at(gameboy);
        REQUIRE(c.found);
        const int col = chick_col(c);
        for (int side = 0; side < 2 && !tested; ++side) {
            const int next = side == 0 ? col - 1 : col + 1;
            if (next < 0 || next >= kGridCols || !cell_blocked(gameboy, c, 0, next)) {
                continue;
            }
            tap(gameboy, side == 0 ? gb::Button::Left : gb::Button::Right);
            const Chick after = chick_at(gameboy);
            REQUIRE(after.x == c.x);
            REQUIRE(after.y == c.y);
            tested = true;
        }
        if (tested) {
            break;
        }
        const int move = plan_move(gameboy, c, 7);
        if (move == kNoMove) {
            break;
        }
        tap(gameboy, button_for(move));
    }
    REQUIRE(tested);
}

TEST_CASE("crossy_camera_streams_new_lanes") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    const int reached = autopilot(gameboy, 13, 60);
    REQUIRE(reached >= 12);
    // nothing but contract terrain ever reaches the screen, so no ring slot went stale
    REQUIRE(play_area_is_all_terrain(gameboy));
    REQUIRE(chick_at(gameboy).found);
}

TEST_CASE("crossy_score_counts_furthest_lane") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    REQUIRE(hud_score(gameboy) == 0);

    const int reached = autopilot(gameboy, 13, 60);
    REQUIRE(reached >= 12);

    const std::vector<DigitRun> digits = hud_digits(gameboy);
    REQUIRE(digits.size() == 2u);
    for (const DigitRun& digit : digits) {
        REQUIRE(digit.pixels > 20u);
        REQUIRE(digit.x1 - digit.x0 >= 5);
    }

    // the camera never retreats and the score is the furthest lane, so a step back holds it
    tap(gameboy, gb::Button::Down);
    REQUIRE(hud_score(gameboy) == reached);
}

TEST_CASE("crossy_play_is_deterministic") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy first;
    gb::Gameboy second;
    start_play(first, rom);
    start_play(second, rom);

    // the same hop script twice: three forward, one sideways, repeat
    for (int step = 0; step < 16; ++step) {
        const gb::Button button = (step % 4 == 3) ? gb::Button::Right : gb::Button::Up;
        tap(first, button);
        tap(second, button);
    }

    const std::span<const uint8_t> a = first.framebuffer();
    const std::span<const uint8_t> b = second.framebuffer();
    REQUIRE(a.size() == b.size());
    REQUIRE(std::equal(a.begin(), a.end(), b.begin()));
    REQUIRE(hud_score(first) == hud_score(second));
}

#include "cartridge.hpp"
#include "gameboy.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
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

// the tile contract: terrain on the bg, chick, cars and digits as sprites
constexpr uint8_t kGrassTileId = 0xA0;
constexpr uint8_t kTreeTileId = 0xA1;
constexpr uint8_t kRoadTileId = 0xA2;
constexpr uint8_t kRoadStripeTileId = 0xA3;
constexpr uint8_t kChickTileId = 0xE0;
constexpr uint8_t kChickHopTileId = 0xE1;
constexpr uint8_t kCarFrontTileId = 0xC0;
constexpr uint8_t kCarRearTileId = 0xC1;
constexpr uint8_t kDigitTileId = 0xD0;

// the popup draws from an inverted copy of the font parked at 0x60
constexpr uint8_t kInvFontFirstTile = 0x60;
constexpr uint8_t kInvFontLastTile = 0x9F;

// the popup band covers screen rows 5..11 of 18
constexpr size_t kPopupTopPx = 40;
constexpr size_t kPopupEndPx = 96;
constexpr uint32_t kPopupPromptRow = 10;

// input is ignored for this many frames after a death
constexpr uint32_t kLockoutFrames = 20;

bool is_road_tile(int tile) {
    return tile == kRoadTileId || tile == kRoadStripeTileId;
}

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

// the rng seeds from the hover frame count, so extra hover frames pick another world
void start_play(gb::Gameboy& gameboy, const std::vector<uint8_t>& rom, uint32_t hover_extra = 0) {
    REQUIRE(gameboy.load_rom(rom));
    run(gameboy, kBootFrames + hover_extra);
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

// a car is 8 px tall at a 4 px lane inset, so it never crosses a 16 px band edge
struct CarRun {
    int band;
    int x0;
    int x1;
};

bool is_car_pixel(const gb::Gameboy& gameboy, int x, int y) {
    const std::span<const uint16_t> ids = gameboy.framebuffer_tiles();
    const std::span<const uint8_t> fb = gameboy.framebuffer();
    const size_t i = static_cast<size_t>(y) * gb::kLcdWidth + static_cast<size_t>(x);
    const uint8_t tile = static_cast<uint8_t>(ids[i]);
    return (ids[i] & 0x100u) != 0 && fb[i] != 0 && (tile == kCarFrontTileId || tile == kCarRearTileId);
}

constexpr int kBands = 9;
constexpr int kBandPx = 16;

std::vector<CarRun> car_runs(const gb::Gameboy& gameboy) {
    std::vector<CarRun> runs;
    for (int band = 0; band < kBands; ++band) {
        int open = -1;
        for (int x = 0; x <= static_cast<int>(gb::kLcdWidth); ++x) {
            bool lit = false;
            for (int y = band * kBandPx; y < band * kBandPx + kBandPx && !lit; ++y) {
                lit = x < static_cast<int>(gb::kLcdWidth) && is_car_pixel(gameboy, x, y);
            }
            if (lit && open < 0) {
                open = x;
            }
            if (!lit && open >= 0) {
                runs.push_back(CarRun{band, open, x - 1});
                open = -1;
            }
        }
    }
    return runs;
}

// the game centers a car on its oam x, so an unclipped run of 16 gives that number back
double car_center(const CarRun& run) {
    if (run.x0 == 0 && run.x1 - run.x0 != 15) {
        return run.x1 - 7;
    }
    return run.x0 + 8;
}

// the two cars of a lane sit half a track apart, so one number describes the lane
constexpr double kCarPeriod = 128.0;

double phase_delta(double a, double b) {
    double d = std::fmod(a - b, kCarPeriod);
    if (d < -kCarPeriod / 2) {
        d += kCarPeriod;
    }
    if (d >= kCarPeriod / 2) {
        d -= kCarPeriod;
    }
    return d;
}

constexpr double kNoPhase = -1000.0;

double lane_phase(const gb::Gameboy& gameboy, int band) {
    for (const CarRun& run : car_runs(gameboy)) {
        if (run.band == band) {
            return std::fmod(car_center(run), kCarPeriod);
        }
    }
    return kNoPhase;
}

int chick_band(const Chick& c) {
    return c.y / kBandPx;
}

// contiguous road lanes ahead of the chick, counted from the next lane up
int road_chunk_ahead(const gb::Gameboy& gameboy, const Chick& c, int gcol) {
    int n = 0;
    while (n < 4 && is_road_tile(cell_tile(gameboy, c, n + 1, gcol))) {
        ++n;
    }
    return n;
}

// one tap is two press frames plus the settle, and the hop commits on the first of them
constexpr double kTapFrames = 14.0;
constexpr uint32_t kMeasureFrames = 16;
// the game kills at 10 px; the plan keeps this much clearance through the whole crossing
constexpr double kPlanMargin = 18.0;
// and the target lane is at least this clear at the moment the hop into it starts
constexpr double kHopMargin = 24.0;

struct Traffic {
    double phase;
    double v;
};

bool burst_is_safe(const std::vector<Traffic>& lanes, double x) {
    for (size_t i = 0; i < lanes.size(); ++i) {
        const double enter = static_cast<double>(i) * kTapFrames;
        const double leave = enter + kTapFrames;
        if (std::fabs(phase_delta(lanes[i].phase + lanes[i].v * enter, x)) < kHopMargin) {
            return false;
        }
        for (double t = enter - 4; t <= leave + 6; t += 1.0) {
            if (std::fabs(phase_delta(lanes[i].phase + lanes[i].v * t, x)) < kPlanMargin) {
                return false;
            }
        }
    }
    return true;
}

// speeds are eighths of a pixel per frame, so the sample snaps back to the exact one
double snap_speed(double v) {
    return std::round(v * 8.0) / 8.0;
}

bool read_traffic(const gb::Gameboy& gameboy, const Chick& c, int n, std::vector<double>& out) {
    out.clear();
    for (int k = 1; k <= n; ++k) {
        const double phase = lane_phase(gameboy, chick_band(c) - k);
        if (phase == kNoPhase) {
            return false;
        }
        out.push_back(phase);
    }
    return true;
}

constexpr int kBurstAttempts = 60;
constexpr uint32_t kBurstWaitFrames = 4;

// crosses a whole road chunk in one burst, so the chick never idles on asphalt
bool try_burst(gb::Gameboy& gameboy, int n) {
    Chick c = chick_at(gameboy);
    std::vector<double> first;
    std::vector<double> second;
    std::vector<Traffic> lanes;
    if (!c.found || !read_traffic(gameboy, c, n, first)) {
        return false;
    }
    // two samples of the same safe grass lane give every road lane its speed
    run(gameboy, kMeasureFrames);
    c = chick_at(gameboy);
    if (!c.found || !read_traffic(gameboy, c, n, second)) {
        return false;
    }
    for (int i = 0; i < n; ++i) {
        lanes.push_back(
            Traffic{second[static_cast<size_t>(i)],
                    snap_speed(phase_delta(second[static_cast<size_t>(i)], first[static_cast<size_t>(i)]) /
                               kMeasureFrames)});
    }

    for (int attempt = 0; attempt < kBurstAttempts; ++attempt) {
        c = chick_at(gameboy);
        if (!c.found || !read_traffic(gameboy, c, n, second)) {
            return false;
        }
        for (int i = 0; i < n; ++i) {
            lanes[static_cast<size_t>(i)].phase = second[static_cast<size_t>(i)];
        }
        if (burst_is_safe(lanes, chick_col(c) * kCellPx + 8)) {
            for (int i = 0; i <= n; ++i) {
                tap(gameboy, gb::Button::Up);
            }
            return true;
        }
        run(gameboy, kBurstWaitFrames);
    }
    return false;
}

// the nearest column whose chunk exit is clear, reachable along this lane
int sidestep_move(const gb::Gameboy& gameboy, const Chick& c, int gcol, int n) {
    for (int d = 1; d < kGridCols; ++d) {
        for (int s = 0; s < 2; ++s) {
            const int j = s == 0 ? gcol - d : gcol + d;
            const int step = j > gcol ? 1 : -1;
            bool clear = j >= 0 && j < kGridCols && !cell_blocked(gameboy, c, n + 1, j);
            for (int t = gcol + step; clear && t != j + step; t += step) {
                clear = !cell_blocked(gameboy, c, 0, t);
            }
            if (clear) {
                return step > 0 ? kRight : kLeft;
            }
        }
    }
    return kNoMove;
}

// a step to either side of a safe lane, keeping the chunk's exit column clear
bool shuffle_column(gb::Gameboy& gameboy, const Chick& c, int gcol, int n) {
    for (int s = 0; s < 2; ++s) {
        const int j = s == 0 ? gcol + 1 : gcol - 1;
        if (j < 0 || j >= kGridCols || cell_blocked(gameboy, c, 0, j) || cell_blocked(gameboy, c, n + 1, j)) {
            continue;
        }
        tap(gameboy, j > gcol ? gb::Button::Right : gb::Button::Left);
        return true;
    }
    return false;
}

// one autopilot action: a plain hop, a sidestep, or a whole road chunk crossed at once
bool autopilot_step(gb::Gameboy& gameboy) {
    const Chick c = chick_at(gameboy);
    if (!c.found) {
        return false;
    }
    const int gcol = chick_col(c);
    if (gcol < 0 || gcol >= kGridCols) {
        return false;
    }
    const int n = road_chunk_ahead(gameboy, c, gcol);
    if (n == 0) {
        const int move = plan_move(gameboy, c, 7);
        if (move == kNoMove) {
            return false;
        }
        tap(gameboy, button_for(move));
        return true;
    }
    if (cell_blocked(gameboy, c, n + 1, gcol)) {
        const int move = sidestep_move(gameboy, c, gcol, n);
        if (move == kNoMove) {
            return false;
        }
        tap(gameboy, button_for(move));
        return true;
    }
    if (try_burst(gameboy, n)) {
        return true;
    }
    // no window from this column; another one lines the lanes up differently
    return shuffle_column(gameboy, c, gcol, n);
}

// steers up the guaranteed path until the hud reaches target or the steps run out
int autopilot(gb::Gameboy& gameboy, int target, int steps) {
    for (int i = 0; i < steps; ++i) {
        if (!autopilot_step(gameboy)) {
            break;
        }
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
            if (tile != kNoTile && tile != kGrassTileId && tile != kTreeTileId && !is_road_tile(tile)) {
                return false;
            }
        }
    }
    return true;
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

bool row_has_tile(const gb::Gameboy& gameboy, uint32_t row, uint8_t tile) {
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

bool row_has_nonzero_digit(const gb::Gameboy& gameboy, uint32_t row) {
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

constexpr size_t kSaveBestOffset = 4;

uint16_t sram_best(std::span<const uint8_t> ram) {
    return static_cast<uint16_t>(ram[kSaveBestOffset] | (ram[kSaveBestOffset + 1] << 8));
}

bool sram_has_magic(std::span<const uint8_t> ram) {
    return ram.size() > kSaveBestOffset + 1 && ram[0] == 'C' && ram[1] == 'R' && ram[2] == 'S' &&
           ram[3] == 'Y';
}

void press(gb::Gameboy& gameboy, gb::Button button, uint32_t frames) {
    gameboy.set_button(button, true);
    run(gameboy, frames);
    gameboy.set_button(button, false);
}

// the popup ignores input for a lockout, so wait it out before the dismissing press
void dismiss(gb::Gameboy& gameboy, gb::Button button) {
    run(gameboy, kLockoutFrames + 4);
    press(gameboy, button, 2);
    // twice the usual settle: the lcd is off while the hover screen is redrawn
    run(gameboy, 2 * kEnterPlayFrames);
}

// hops onto the next road lane and stands there until the traffic finds the chick
void die_under_a_car(gb::Gameboy& gameboy, int steps) {
    for (int i = 0; i < steps; ++i) {
        const Chick c = chick_at(gameboy);
        REQUIRE(c.found);
        if (road_chunk_ahead(gameboy, c, chick_col(c)) > 0) {
            tap(gameboy, gb::Button::Up);
            break;
        }
        REQUIRE(autopilot_step(gameboy));
    }
    for (uint32_t i = 0; i < 900; ++i) {
        gameboy.run_frame();
        if (popup_shown(gameboy)) {
            run(gameboy, 6);
            return;
        }
    }
    FAIL("no car ever reached the chick");
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
    // nothing but contract terrain is on screen, so the world is up before the first hop
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

TEST_CASE("roads_appear_and_read_distinct") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    bool found = false;
    for (int step = 0; step < 20 && !found; ++step) {
        const Chick c = chick_at(gameboy);
        REQUIRE(c.found);
        for (int k = 0; k <= 6 && !found; ++k) {
            int road_cells = 0;
            int tree_cells = 0;
            for (int col = 0; col < kGridCols; ++col) {
                const int tile = cell_tile(gameboy, c, k, col);
                road_cells += is_road_tile(tile) ? 1 : 0;
                tree_cells += tile == kTreeTileId ? 1 : 0;
            }
            if (road_cells == 0) {
                continue;
            }
            // a road lane is asphalt end to end: always crossable, never a tree
            REQUIRE(road_cells == kGridCols);
            REQUIRE(tree_cells == 0);
            found = true;
        }
        if (!found) {
            REQUIRE(autopilot_step(gameboy));
        }
    }
    REQUIRE(found);
    REQUIRE(bg_has_tile(gameboy, kRoadTileId));
    REQUIRE(bg_has_tile(gameboy, kRoadStripeTileId));
}

TEST_CASE("cars_move_and_keep_their_gap") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    std::vector<CarRun> runs = car_runs(gameboy);
    REQUIRE_FALSE(runs.empty());
    const int band = runs.front().band;

    double phase = lane_phase(gameboy, band);
    REQUIRE(phase != kNoPhase);
    double travelled = 0;
    int sign = 0;
    for (uint32_t frame = 0; frame < 120; ++frame) {
        gameboy.run_frame();
        const double now = lane_phase(gameboy, band);
        REQUIRE(now != kNoPhase);
        const double step = phase_delta(now, phase);
        // 8.8 fixed point at under a pixel a frame, so a step is never a jump
        REQUIRE(std::fabs(step) <= 1.0);
        if (step != 0) {
            if (sign == 0) {
                sign = step > 0 ? 1 : -1;
            }
            REQUIRE((step > 0 ? 1 : -1) == sign);
        }
        travelled += step;
        phase = now;

        // the two cars share the lane's speed, so their gap can never close
        std::vector<double> centers;
        for (const CarRun& run : car_runs(gameboy)) {
            if (run.band == band) {
                centers.push_back(car_center(run));
            }
        }
        for (size_t i = 1; i < centers.size(); ++i) {
            for (size_t j = 0; j < i; ++j) {
                REQUIRE(std::fabs(centers[i] - centers[j]) >= 40.0);
            }
        }
    }
    REQUIRE(std::fabs(travelled) >= 40.0);
}

TEST_CASE("car_kills_the_chick") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    die_under_a_car(gameboy, 20);

    REQUIRE(popup_shown(gameboy));
    // the chick may have died right where the popup sits, so it is parked offscreen
    REQUIRE_FALSE(chick_at(gameboy).found);
    REQUIRE(car_runs(gameboy).empty());
    for (char c : std::string("GAMEOVRSCBTPY")) {
        REQUIRE(bg_has_tile(gameboy, popup_tile(c)));
    }
}

TEST_CASE("popup_is_centered_and_solid") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    die_under_a_car(gameboy, 20);

    const std::span<const uint16_t> ids = gameboy.framebuffer_tiles();
    for (size_t y = kPopupTopPx; y < kPopupEndPx; ++y) {
        for (size_t x = 0; x < gb::kLcdWidth; ++x) {
            const uint16_t id = ids[y * gb::kLcdWidth + x];
            // the whole band is popup fill or an inverted glyph: no world, no sprite, one colour
            REQUIRE((id & 0x100u) == 0);
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
            const uint8_t tile = static_cast<uint8_t>(id);
            if ((id & 0x100u) != 0 || tile < kGrassTileId || tile > kRoadStripeTileId) {
                continue;
            }
            above = above || y < kPopupTopPx;
            below = below || y >= kPopupEndPx;
        }
    }
    REQUIRE(above);
    REQUIRE(below);

    // an even length prompt lands symmetric around the 20 column grid
    const auto [left, right] = glyph_span(gameboy, kPopupPromptRow, kInvFontFirstTile + 1, kInvFontLastTile);
    REQUIRE(left >= 0);
    REQUIRE(left + right == 19);
}

TEST_CASE("best_score_lands_in_sram") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    die_under_a_car(gameboy, 20);

    const std::span<uint8_t> ram = gameboy.external_ram();
    REQUIRE(sram_has_magic(ram));
    REQUIRE(sram_best(ram) >= 1u);
}

TEST_CASE("best_survives_reload") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy first;
    start_play(first, rom);
    die_under_a_car(first, 20);
    const std::vector<uint8_t> saved(first.external_ram().begin(), first.external_ram().end());
    const uint16_t best = sram_best(saved);
    REQUIRE(best >= 1u);

    gb::Gameboy second;
    REQUIRE(second.load_rom(rom));
    const std::span<uint8_t> ram = second.external_ram();
    REQUIRE(ram.size() == saved.size());
    std::copy(saved.begin(), saved.end(), ram.begin());

    // boot must read the saved best instead of re-initialising it
    run(second, kBootFrames);
    REQUIRE(sram_has_magic(second.external_ram()));
    REQUIRE(sram_best(second.external_ram()) == best);
    REQUIRE(row_has_nonzero_digit(second, kBestRow));
}

TEST_CASE("retry_flow") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    die_under_a_car(gameboy, 20);
    REQUIRE(popup_shown(gameboy));

    // an early press is inside the lockout, so the popup stays put
    press(gameboy, gb::Button::A, 2);
    run(gameboy, 2 * kEnterPlayFrames);
    REQUIRE(popup_shown(gameboy));
    REQUIRE_FALSE(title_shown(gameboy));

    // b is neither the start nor the hop key, and it still clears the popup
    dismiss(gameboy, gb::Button::B);
    REQUIRE_FALSE(popup_shown(gameboy));
    REQUIRE(row_has_tile(gameboy, kTitleRow, font_tile('C')));
    REQUIRE(row_has_tile(gameboy, kTitleRow, font_tile('Y')));
    REQUIRE(row_has_nonzero_digit(gameboy, kBestRow));

    // the press that cleared the popup is spent: the hover screen just keeps hovering
    for (uint32_t i = 0; i < 60; ++i) {
        gameboy.run_frame();
        REQUIRE(title_shown(gameboy));
        REQUIRE(chick_at(gameboy).found);
    }

    press(gameboy, gb::Button::A, 2);
    run(gameboy, 2 * kEnterPlayFrames);
    REQUIRE_FALSE(title_shown(gameboy));
    REQUIRE_FALSE(popup_shown(gameboy));
    REQUIRE(hud_score(gameboy) == 0);
    const Chick fresh = chick_at(gameboy);
    REQUIRE(fresh.found);
    REQUIRE(chick_col(fresh) == 4);
}

TEST_CASE("autopilot_crosses_roads") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    // a handful of fixed seeds: still deterministic, but not one lucky world
    for (uint32_t hover_extra : {0u, 1u, 2u, 5u, 11u, 17u, 23u, 31u}) {
        gb::Gameboy gameboy;
        start_play(gameboy, rom, hover_extra);

        const int reached = autopilot(gameboy, 13, 60);
        REQUIRE(reached >= 12);
        // the whole mixed run happened without a scratch
        REQUIRE(chick_at(gameboy).found);
        REQUIRE_FALSE(popup_shown(gameboy));
        REQUIRE(play_area_is_all_terrain(gameboy));
    }
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

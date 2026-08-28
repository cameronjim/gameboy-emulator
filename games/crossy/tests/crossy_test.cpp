#include "cartridge.hpp"
#include "gameboy.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
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
constexpr uint8_t kWaterTileId = 0xA4;
constexpr uint8_t kRailTileId = 0xA5;
constexpr uint8_t kRailWarnTileId = 0xA6;
constexpr uint8_t kChickTileId = 0xE0;
constexpr uint8_t kChickHopTileId = 0xE1;
constexpr uint8_t kCarFrontTileId = 0xC0;
constexpr uint8_t kCarRearTileId = 0xC1;
constexpr uint8_t kLogTileId = 0xC4;
constexpr uint8_t kTrainFirstTileId = 0xC8;
constexpr uint8_t kTrainLastTileId = 0xCB;
constexpr uint8_t kDigitTileId = 0xD0;
constexpr uint8_t kEagleFirstTileId = 0xCC;
constexpr uint8_t kEagleLastTileId = 0xCD;

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

bool is_water_tile(int tile) {
    return tile == kWaterTileId;
}

bool is_track_tile(int tile) {
    return tile == kRailTileId || tile == kRailWarnTileId;
}

// map column of the warning light, the one cell of a track lane whose art blinks
constexpr int kWarnTileCol = 10;

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

// top-left corner of the eagle's lit sprite pixels, wherever the pair has reached
Chick eagle_at(const gb::Gameboy& gameboy) {
    const std::span<const uint16_t> ids = gameboy.framebuffer_tiles();
    const std::span<const uint8_t> fb = gameboy.framebuffer();
    Chick e{0, 0, false, false};
    for (size_t i = 0; i < ids.size(); ++i) {
        const uint8_t tile = static_cast<uint8_t>(ids[i]);
        if ((ids[i] & 0x100u) == 0 || fb[i] == 0) {
            continue;
        }
        if (tile < kEagleFirstTileId || tile > kEagleLastTileId) {
            continue;
        }
        const int x = static_cast<int>(i % gb::kLcdWidth);
        const int y = static_cast<int>(i / gb::kLcdWidth);
        if (!e.found || x < e.x) {
            e.x = x;
        }
        if (!e.found || y < e.y) {
            e.y = y;
        }
        e.found = true;
    }
    return e;
}

int chick_col(const Chick& c) {
    return (c.x - kCellInset) / kCellPx;
}

// chick_at reports the leftmost lit pixel, one in from the 8 px sprite's left edge
constexpr int kChickLitInset = 3;

double chick_center(const Chick& c) {
    return c.x + kChickLitInset;
}

// the grid cell the chick's center sits in; a ride leaves it between two columns
int chick_cell(const Chick& c) {
    const int cell = (c.x + kChickLitInset) / kCellPx;
    return cell < 0 ? 0 : (cell >= kGridCols ? kGridCols - 1 : cell);
}

// the bg tile of the grid cell k lanes ahead of the chick, in grid column gcol
int cell_tile(const gb::Gameboy& gameboy, const Chick& c, int k, int gcol) {
    // the chick sits higher in a water lane, so its lane is read off the 16 px band, not its inset
    const int row = (c.y / kCellPx) * 2 - 2 * k;
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

// a cell the chick can simply stand on: neither a tree nor open water
bool cell_landable(const gb::Gameboy& gameboy, const Chick& c, int k, int gcol) {
    const int tile = cell_tile(gameboy, c, k, gcol);
    return tile != kTreeTileId && !is_water_tile(tile);
}

// water needs a log and rails need a timed window, so the dry land planner routes around both
bool cell_impassable(const gb::Gameboy& gameboy, const Chick& c, int k, int gcol) {
    const int tile = cell_tile(gameboy, c, k, gcol);
    return tile == kTreeTileId || is_water_tile(tile) || is_track_tile(tile);
}

// the warning light's own 8x8 cell, k lanes ahead of the chick
int warn_cell(const gb::Gameboy& gameboy, const Chick& c, int k) {
    return bg_cell(gameboy, kWarnTileCol, (c.y / kCellPx) * 2 - 2 * k);
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
            if (first[at] != kUnseen || cell_impassable(gameboy, c, nk, nc)) {
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

// a mover is 8 px tall at a 4 px lane inset, so it never crosses a 16 px band edge
struct CarRun {
    int band;
    int x0;
    int x1;
};

constexpr int kBands = 9;
constexpr int kBandPx = 16;

// columns of one sprite family lit in a band; bridge spans the hole a rider punches in a log
std::vector<CarRun> runs_of(const gb::Gameboy& gameboy, uint8_t lo, uint8_t hi, int bridge) {
    const std::span<const uint16_t> ids = gameboy.framebuffer_tiles();
    const std::span<const uint8_t> fb = gameboy.framebuffer();
    std::vector<CarRun> runs;
    for (int band = 0; band < kBands; ++band) {
        int open = -1;
        int shut = -1;
        for (int x = 0; x <= static_cast<int>(gb::kLcdWidth) + bridge; ++x) {
            bool lit = false;
            for (int y = band * kBandPx; y < band * kBandPx + kBandPx && !lit; ++y) {
                if (x >= static_cast<int>(gb::kLcdWidth)) {
                    break;
                }
                const size_t i = static_cast<size_t>(y) * gb::kLcdWidth + static_cast<size_t>(x);
                const uint8_t tile = static_cast<uint8_t>(ids[i]);
                lit = (ids[i] & 0x100u) != 0 && fb[i] != 0 && tile >= lo && tile <= hi;
            }
            if (lit) {
                if (open < 0) {
                    open = x;
                }
                shut = x;
            } else if (open >= 0 && x - shut > bridge) {
                runs.push_back(CarRun{band, open, shut});
                open = -1;
            }
        }
    }
    return runs;
}

std::vector<CarRun> car_runs(const gb::Gameboy& gameboy) {
    return runs_of(gameboy, kCarFrontTileId, kCarRearTileId, 0);
}

// the chick's opaque pixels are six wide, so eight blank columns bridge a rider's hole
constexpr int kRiderBridge = 8;

std::vector<CarRun> log_runs(const gb::Gameboy& gameboy) {
    return runs_of(gameboy, kLogTileId, kLogTileId, kRiderBridge);
}

// the train is one solid block, but a chick standing on the rails punches the same hole a rider does
std::vector<CarRun> train_runs(const gb::Gameboy& gameboy) {
    return runs_of(gameboy, kTrainFirstTileId, kTrainLastTileId, kRiderBridge);
}

bool train_in_band(const gb::Gameboy& gameboy, int band) {
    for (const CarRun& run : train_runs(gameboy)) {
        if (run.band == band) {
            return true;
        }
    }
    return false;
}

// the game centers a car on its oam x, so an unclipped run of 16 gives that number back
double car_center(const CarRun& run) {
    if (run.x0 == 0 && run.x1 - run.x0 != 15) {
        return run.x1 - 7;
    }
    return run.x0 + 8;
}

// a log is 24 px centered on its track x; a run clipped at the left edge is read from its right
constexpr int kLogPx = 24;

double log_center(const CarRun& run) {
    if (run.x0 == 0 && run.x1 - run.x0 + 1 < kLogPx) {
        return run.x1 - (kLogPx / 2 - 1);
    }
    return run.x0 + kLogPx / 2;
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

// the two logs of a lane sit half a track apart too, so either one gives the same phase
double log_phase(const gb::Gameboy& gameboy, int band) {
    for (const CarRun& run : log_runs(gameboy)) {
        if (run.band == band) {
            return std::fmod(log_center(run), kCarPeriod);
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
            bool clear = j >= 0 && j < kGridCols && !cell_impassable(gameboy, c, n + 1, j);
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
        if (j < 0 || j >= kGridCols || cell_blocked(gameboy, c, 0, j) ||
            cell_impassable(gameboy, c, n + 1, j)) {
            continue;
        }
        tap(gameboy, j > gcol ? gb::Button::Right : gb::Button::Left);
        return true;
    }
    return false;
}

// logs are slower than cars, so a longer baseline keeps the sampled speed on a whole 32nd
constexpr uint32_t kLogMeasureFrames = 24;
// the hop commits on the tap's first frame and its slide lands eight frames later
constexpr double kHopArrivalFrames = 9.0;
// the game rides within 12 px of a log center; the plan hops with six of those to spare
constexpr double kRideCatchPx = 12.0;
constexpr double kRideMargin = 6.0;
// the game kills a ride that reaches these, so a landing must leave room downstream of it
constexpr double kRideLeftEdge = 4.0;
constexpr double kRideRightEdge = 156.0;
constexpr double kLandRoom = 60.0;
// past these the ride is nearly over, so the chick gives up and heads back
constexpr double kBailLo = 20.0;
constexpr double kBailHi = 140.0;
// the boarding column: upstream of the drift, so the whole screen is left to ride across
constexpr int kBoardColLeft = 3;
constexpr int kBoardColRight = 6;
// one log passes a fixed x every 128/speed frames, so a wait is at most a few hundred
constexpr int kRideAttempts = 800;
constexpr int kRetreatAttempts = 400;

// two samples of the lane give its logs their shared speed, in whole 32nds of a pixel
bool measure_logs(gb::Gameboy& gameboy, int band, double& v) {
    const double before = log_phase(gameboy, band);
    if (before == kNoPhase) {
        return false;
    }
    run(gameboy, kLogMeasureFrames);
    const double after = log_phase(gameboy, band);
    if (after == kNoPhase) {
        return false;
    }
    v = std::round(phase_delta(after, before) / kLogMeasureFrames * 32.0) / 32.0;
    return v != 0.0;
}

// water needs room to drift, so the chick lines up in a middle column before it boards
void approach_column(gb::Gameboy& gameboy, int want) {
    for (int guard = 0; guard < kGridCols; ++guard) {
        const Chick c = chick_at(gameboy);
        if (!c.found) {
            return;
        }
        const int gcol = chick_cell(c);
        if (gcol == want) {
            return;
        }
        const int step = want > gcol ? 1 : -1;
        if (cell_blocked(gameboy, c, 0, gcol + step)) {
            return;
        }
        tap(gameboy, step > 0 ? gb::Button::Right : gb::Button::Left);
    }
}

// contiguous water lanes ahead of the chick, counted from the next lane up
int water_chunk_ahead(const gb::Gameboy& gameboy, const Chick& c) {
    int n = 0;
    while (n < 3 && is_water_tile(cell_tile(gameboy, c, n + 1, 0))) {
        ++n;
    }
    return n;
}

// one lane forward (dk 1) or back (dk -1), taken as soon as the landing is a clear column or a log
bool hop_lane(gb::Gameboy& gameboy, int dk, double v, int budget) {
    const Chick from = chick_at(gameboy);
    if (!from.found) {
        return false;
    }
    // the landing lands within a log's width of the chick, so its own column caps the ride's room
    const double reach = v > 0 ? kRideRightEdge - chick_center(from) : chick_center(from) - kRideLeftEdge;
    const double need = std::min(kLandRoom, reach - kRideCatchPx);

    for (int attempt = 0; attempt < budget; ++attempt) {
        const Chick c = chick_at(gameboy);
        if (!c.found) {
            return false;
        }
        const int gcol = chick_cell(c);
        const double cx = chick_center(c);
        bool go = !is_water_tile(cell_tile(gameboy, c, dk, gcol)) && !cell_blocked(gameboy, c, dk, gcol);
        // log motion is linear, so one speed and one sighting place it at the hop's arrival
        for (const CarRun& log : log_runs(gameboy)) {
            if (go || log.band != chick_band(c) - dk) {
                continue;
            }
            const double at = log_center(log) + v * kHopArrivalFrames;
            // and the landing must leave room downstream, or the ride ends at the screen edge
            const double room = v > 0 ? kRideRightEdge - at : at - kRideLeftEdge;
            go = std::fabs(at - cx) <= kRideCatchPx - kRideMargin && room >= need;
        }
        if (go) {
            tap(gameboy, dk > 0 ? gb::Button::Up : gb::Button::Down);
            return true;
        }
        if (is_water_tile(cell_tile(gameboy, c, 0, gcol)) && (cx < kBailLo || cx > kBailHi)) {
            return false;
        }
        gameboy.run_frame();
    }
    return false;
}

// crosses a whole water chunk: every speed read from the bank, then one log at a time
bool water_cross(gb::Gameboy& gameboy) {
    const Chick start = chick_at(gameboy);
    if (!start.found) {
        return false;
    }
    // a failed exit can leave the chick afloat, with no measurement to hand
    if (is_water_tile(cell_tile(gameboy, start, 0, chick_cell(start)))) {
        double afloat_v = 0;
        if (!is_water_tile(cell_tile(gameboy, start, 1, chick_cell(start)))) {
            return hop_lane(gameboy, 1, 0, kRideAttempts);
        }
        return measure_logs(gameboy, chick_band(start) - 1, afloat_v) &&
               hop_lane(gameboy, 1, afloat_v, kRideAttempts);
    }

    const int n = water_chunk_ahead(gameboy, start);
    std::vector<double> v;
    for (int k = 0; k < n; ++k) {
        double lane_v = 0;
        // measured from the bank, where a long baseline costs the chick nothing
        if (!measure_logs(gameboy, chick_band(start) - 1 - k, lane_v)) {
            return false;
        }
        v.push_back(lane_v);
    }
    if (v.empty()) {
        return false;
    }
    approach_column(gameboy, v.front() > 0 ? kBoardColLeft : kBoardColRight);

    for (int k = 0; k < n; ++k) {
        if (hop_lane(gameboy, 1, v[static_cast<size_t>(k)], kRideAttempts)) {
            continue;
        }
        // carried too far with nothing lined up: back to the bank, log by log, and start over
        for (int back = k; back > 0; --back) {
            if (!hop_lane(gameboy, -1, back > 1 ? v[static_cast<size_t>(back - 2)] : 0.0, kRetreatAttempts)) {
                return false;
            }
        }
        return k > 0;
    }
    // dry land ahead, so the exit only waits on a column clear of trees
    return hop_lane(gameboy, 1, 0, kRideAttempts);
}

// the warning blinks every 15 frames, so a light that holds its crossbuck this long is quiet
constexpr uint32_t kQuietProof = 20;
// a whole warning plus a whole sweep is barely a hundred frames, well short of the eagle's patience
constexpr uint32_t kQuietWaitFrames = 320;

bool track_looks_quiet(const gb::Gameboy& gameboy, int k) {
    const Chick c = chick_at(gameboy);
    return c.found && warn_cell(gameboy, c, k) == kRailWarnTileId &&
           !train_in_band(gameboy, chick_band(c) - k);
}

// holds still until the track k lanes ahead has proved itself between sweeps
bool wait_for_quiet(gb::Gameboy& gameboy, int k) {
    uint32_t held = 0;
    for (uint32_t frame = 0; frame < kQuietWaitFrames; ++frame) {
        if (!chick_at(gameboy).found) {
            return false;
        }
        held = track_looks_quiet(gameboy, k) ? held + 1 : 0;
        if (held >= kQuietProof) {
            return true;
        }
        gameboy.run_frame();
    }
    return false;
}

// rails are grass on a timer: crossed only from a proven quiet light, and never stood on after
bool track_cross(gb::Gameboy& gameboy) {
    const Chick c = chick_at(gameboy);
    if (!c.found) {
        return false;
    }
    const int gcol = chick_cell(c);
    // the lane past a track is always grass, so the crossing only needs a column clear of trees
    if (cell_impassable(gameboy, c, 2, gcol)) {
        const int move = sidestep_move(gameboy, c, gcol, 1);
        if (move == kNoMove) {
            return false;
        }
        tap(gameboy, button_for(move));
        return true;
    }
    if (!wait_for_quiet(gameboy, 1)) {
        return false;
    }
    tap(gameboy, gb::Button::Up);
    tap(gameboy, gb::Button::Up);
    return true;
}

// one autopilot action: a plain hop, a sidestep, a road chunk crossed at once, or a river ridden
bool autopilot_step(gb::Gameboy& gameboy) {
    const Chick c = chick_at(gameboy);
    if (!c.found) {
        return false;
    }
    const int gcol = chick_cell(c);
    if (is_track_tile(cell_tile(gameboy, c, 0, gcol))) {
        // standing on rails is never safe, so the exit hop outranks anything the planner wants
        if (cell_landable(gameboy, c, 1, gcol)) {
            tap(gameboy, gb::Button::Up);
            return true;
        }
        const int move = sidestep_move(gameboy, c, gcol, 0);
        if (move == kNoMove) {
            return false;
        }
        tap(gameboy, button_for(move));
        return true;
    }
    if (is_track_tile(cell_tile(gameboy, c, 1, gcol))) {
        return track_cross(gameboy);
    }
    if (is_water_tile(cell_tile(gameboy, c, 0, gcol)) || is_water_tile(cell_tile(gameboy, c, 1, gcol))) {
        return water_cross(gameboy);
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

// autopilots up to the bank, then catches the first log that comes by and stops there
bool board_a_log(gb::Gameboy& gameboy, int steps) {
    for (int i = 0; i < steps; ++i) {
        const Chick c = chick_at(gameboy);
        if (!c.found) {
            return false;
        }
        if (is_water_tile(cell_tile(gameboy, c, 0, chick_cell(c)))) {
            return true;
        }
        if (!is_water_tile(cell_tile(gameboy, c, 1, chick_cell(c)))) {
            if (!autopilot_step(gameboy)) {
                return false;
            }
            continue;
        }
        // water_cross would carry straight on over the whole chunk, so board by hand
        double v = 0;
        if (!measure_logs(gameboy, chick_band(c) - 1, v)) {
            return false;
        }
        approach_column(gameboy, v > 0 ? kBoardColLeft : kBoardColRight);
        if (!hop_lane(gameboy, 1, v, kRideAttempts)) {
            return false;
        }
    }
    return false;
}

// autopilots up to the bank of the first track it meets and stops on the lane below it
bool walk_to_track(gb::Gameboy& gameboy, int steps) {
    for (int i = 0; i < steps; ++i) {
        const Chick c = chick_at(gameboy);
        if (!c.found) {
            return false;
        }
        if (is_track_tile(cell_tile(gameboy, c, 1, chick_cell(c)))) {
            return true;
        }
        if (!autopilot_step(gameboy)) {
            return false;
        }
    }
    return false;
}

// sidesteps along the current lane until the column past the track is one the chick can land on
bool line_up_past_track(gb::Gameboy& gameboy) {
    for (int guard = 0; guard < kGridCols; ++guard) {
        const Chick c = chick_at(gameboy);
        if (!c.found) {
            return false;
        }
        if (cell_landable(gameboy, c, 2, chick_cell(c))) {
            return true;
        }
        const int move = sidestep_move(gameboy, c, chick_cell(c), 1);
        if (move == kNoMove) {
            return false;
        }
        tap(gameboy, button_for(move));
    }
    return false;
}

// how many warning cells the whole two row lane carries; the light is a single cell
int warn_cells_in_lane(const gb::Gameboy& gameboy, const Chick& c, int k) {
    const int row = (c.y / kCellPx) * 2 - 2 * k;
    int n = 0;
    for (int dr = 0; dr < 2; ++dr) {
        for (int cx = 0; cx < 20; ++cx) {
            n += bg_cell(gameboy, cx, row + dr) == kRailWarnTileId ? 1 : 0;
        }
    }
    return n;
}

// holds still until the light k lanes ahead drops its crossbuck: the warning has begun
bool wait_for_blink(gb::Gameboy& gameboy, int k, uint32_t budget) {
    for (uint32_t frame = 0; frame < budget; ++frame) {
        const Chick c = chick_at(gameboy);
        if (!c.found) {
            return false;
        }
        if (warn_cell(gameboy, c, k) == kRailTileId) {
            return true;
        }
        gameboy.run_frame();
    }
    return false;
}

// holds still until the sweep itself is on screen k lanes ahead
bool wait_for_train(gb::Gameboy& gameboy, int k, uint32_t budget) {
    for (uint32_t frame = 0; frame < budget; ++frame) {
        const Chick c = chick_at(gameboy);
        if (!c.found) {
            return false;
        }
        if (train_in_band(gameboy, chick_band(c) - k)) {
            return true;
        }
        gameboy.run_frame();
    }
    return false;
}

// every cell of the play area, minus the ones a sprite fully covers
bool play_area_is_all_terrain(const gb::Gameboy& gameboy) {
    for (int cy = 0; cy < 18; ++cy) {
        for (int cx = 0; cx < 20; ++cx) {
            const int tile = bg_cell(gameboy, cx, cy);
            if (tile != kNoTile && tile != kGrassTileId && tile != kTreeTileId && !is_road_tile(tile) &&
                !is_water_tile(tile) && !is_track_tile(tile)) {
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

// runs frames until the popup lands, or gives up
bool wait_for_death(gb::Gameboy& gameboy, uint32_t budget) {
    for (uint32_t frame = 0; frame < budget; ++frame) {
        if (popup_shown(gameboy)) {
            return true;
        }
        gameboy.run_frame();
    }
    return popup_shown(gameboy);
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

// the base tier tops a car out at a whole pixel a frame; the ramp is what beats it
constexpr double kBaseCarMax = 1.0;
constexpr double kSpeedTol = 0.05;

// the quickest car of any visible road lane, in px per frame; -1 when the camera moved mid sample
double fastest_car(gb::Gameboy& gameboy) {
    std::vector<double> before;
    for (int band = 0; band < kBands; ++band) {
        before.push_back(lane_phase(gameboy, band));
    }
    const Chick from = chick_at(gameboy);
    run(gameboy, kMeasureFrames);
    const Chick to = chick_at(gameboy);
    // a creep mid sample slides every lane down a band, so the pairing would be nonsense
    if (!from.found || !to.found || from.y != to.y) {
        return -1.0;
    }
    double best = 0;
    for (int band = 0; band < kBands; ++band) {
        const double now = lane_phase(gameboy, band);
        const double was = before[static_cast<size_t>(band)];
        if (was == kNoPhase || now == kNoPhase) {
            continue;
        }
        best = std::max(best, std::fabs(snap_speed(phase_delta(now, was) / kMeasureFrames)));
    }
    return best;
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

// measured locally: a decayed channel holds one dc level, a hop swings past 6000, a hit past 30000
constexpr int32_t kSilentSwing = 512;
constexpr int32_t kHopSwing = 1000;
constexpr int32_t kDeathSwing = 4000;

void drain_audio(gb::Gameboy& gameboy) {
    std::array<int16_t, 8192> buffer{};
    while (gameboy.read_audio(buffer) != 0) {
    }
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
        // the full autopilot, so a river between here and the next tree is no obstacle
        REQUIRE(autopilot_step(gameboy));
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

// the seed that opens with a road chunk; the default one opens with water
constexpr uint32_t kRoadSeedExtra = 6;

TEST_CASE("cars_move_and_keep_their_gap") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom, kRoadSeedExtra);

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
            if ((id & 0x100u) != 0 || tile < kGrassTileId || tile > kRailWarnTileId) {
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

    // a handful of fixed seeds whose first danger chunk is a road: not one lucky world
    for (uint32_t hover_extra : {6u, 7u, 8u, 9u, 12u, 23u, 24u, 30u}) {
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

TEST_CASE("water_appears_and_reads_distinct") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    bool found = false;
    for (int step = 0; step < 30 && !found; ++step) {
        const Chick c = chick_at(gameboy);
        REQUIRE(c.found);
        for (int k = 0; k <= 6 && !found; ++k) {
            int water_cells = 0;
            int tree_cells = 0;
            for (int col = 0; col < kGridCols; ++col) {
                const int tile = cell_tile(gameboy, c, k, col);
                water_cells += is_water_tile(tile) ? 1 : 0;
                tree_cells += tile == kTreeTileId ? 1 : 0;
            }
            if (water_cells == 0) {
                continue;
            }
            // a water lane is open water end to end: never a tree, never a patch of bank
            REQUIRE(water_cells == kGridCols);
            REQUIRE(tree_cells == 0);
            found = true;
        }
        if (!found) {
            REQUIRE(autopilot_step(gameboy));
        }
    }
    REQUIRE(found);
    REQUIRE(bg_has_tile(gameboy, kWaterTileId));
}

TEST_CASE("logs_move_and_keep_their_gap") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    const std::vector<CarRun> runs = log_runs(gameboy);
    REQUIRE_FALSE(runs.empty());
    const int band = runs.front().band;

    double phase = log_phase(gameboy, band);
    REQUIRE(phase != kNoPhase);
    double travelled = 0;
    int sign = 0;
    for (uint32_t frame = 0; frame < 120; ++frame) {
        gameboy.run_frame();
        const double now = log_phase(gameboy, band);
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

        // the two logs share the lane's speed, so their gap can never close
        std::vector<double> centers;
        for (const CarRun& log : log_runs(gameboy)) {
            if (log.band == band) {
                centers.push_back(log_center(log));
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

TEST_CASE("riding_carries_the_chick") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    REQUIRE(board_a_log(gameboy, 24));

    Chick c = chick_at(gameboy);
    REQUIRE(c.found);
    const int band = chick_band(c);
    const int from = c.x;
    int dir = 0;
    int prev = c.x;
    for (uint32_t frame = 0; frame < 30; ++frame) {
        gameboy.run_frame();
        c = chick_at(gameboy);
        REQUIRE(c.found);
        const int step = c.x - prev;
        if (step != 0) {
            // under a pixel a frame, and never once against the log
            REQUIRE(std::abs(step) <= 1);
            if (dir == 0) {
                dir = step > 0 ? 1 : -1;
            }
            REQUIRE((step > 0 ? 1 : -1) == dir);
        }
        prev = c.x;

        double nearest = 999;
        for (const CarRun& log : log_runs(gameboy)) {
            if (log.band == band) {
                nearest = std::min(nearest, std::fabs(log_center(log) - (chick_center(c))));
            }
        }
        REQUIRE(nearest <= kRideCatchPx);
    }
    REQUIRE(dir != 0);
    // the slowest log still covers twelve px in thirty frames
    REQUIRE(std::abs(prev - from) >= 8);
}

// no log can drift back under the chick from this far off before the hop lands
constexpr double kOpenWaterPx = 40.0;

TEST_CASE("hopping_into_water_drowns") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    bool at_bank = false;
    for (int step = 0; step < 24 && !at_bank; ++step) {
        const Chick c = chick_at(gameboy);
        REQUIRE(c.found);
        if (is_water_tile(cell_tile(gameboy, c, 1, chick_cell(c)))) {
            at_bank = true;
            break;
        }
        REQUIRE(autopilot_step(gameboy));
    }
    REQUIRE(at_bank);

    const int band = chick_band(chick_at(gameboy)) - 1;
    bool jumped = false;
    for (int attempt = 0; attempt < kRideAttempts && !jumped; ++attempt) {
        const Chick c = chick_at(gameboy);
        REQUIRE(c.found);
        double nearest = 999;
        for (const CarRun& log : log_runs(gameboy)) {
            if (log.band == band) {
                nearest = std::min(nearest, std::fabs(log_center(log) - (chick_center(c))));
            }
        }
        if (nearest >= kOpenWaterPx) {
            tap(gameboy, gb::Button::Up);
            jumped = true;
            break;
        }
        gameboy.run_frame();
    }
    REQUIRE(jumped);

    run(gameboy, 6);
    REQUIRE(popup_shown(gameboy));
    REQUIRE_FALSE(chick_at(gameboy).found);
}

TEST_CASE("riding_off_the_edge_kills") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    REQUIRE(board_a_log(gameboy, 24));

    // the slowest log needs three hundred odd frames to carry the chick the width of the screen
    bool killed = false;
    for (uint32_t frame = 0; frame < 900 && !killed; ++frame) {
        gameboy.run_frame();
        killed = popup_shown(gameboy);
    }
    REQUIRE(killed);
    run(gameboy, 6);
    REQUIRE_FALSE(chick_at(gameboy).found);
    REQUIRE(log_runs(gameboy).empty());
}

TEST_CASE("autopilot_crosses_rivers") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    // fixed seeds whose first danger chunk is water, so every run is a mixed crossing
    for (uint32_t hover_extra : {0u, 2u, 4u, 14u, 20u, 34u}) {
        gb::Gameboy gameboy;
        start_play(gameboy, rom, hover_extra);

        const int reached = autopilot(gameboy, 15, 80);
        REQUIRE(reached >= 14);
        REQUIRE(chick_at(gameboy).found);
        REQUIRE_FALSE(popup_shown(gameboy));
        REQUIRE(play_area_is_all_terrain(gameboy));
    }
}

// the eagle's timer is 600 frames; these bracket the swoop that follows it
constexpr uint32_t kIdleSafeFrames = 560;
constexpr uint32_t kEagleWatchFrames = 140;
constexpr uint32_t kSwoopFrames = 140;

TEST_CASE("idling_summons_the_eagle") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    // nine seconds of standing still on the opening grass and the sky is still empty
    run(gameboy, kIdleSafeFrames);
    REQUIRE_FALSE(eagle_at(gameboy).found);
    REQUIRE(chick_at(gameboy).found);
    REQUIRE_FALSE(popup_shown(gameboy));

    Chick eagle{0, 0, false, false};
    for (uint32_t frame = 0; frame < kEagleWatchFrames && !eagle.found; ++frame) {
        gameboy.run_frame();
        eagle = eagle_at(gameboy);
    }
    REQUIRE(eagle.found);
    // it comes down the chick's own column, four px a frame
    const Chick chick = chick_at(gameboy);
    REQUIRE(chick.found);
    REQUIRE(std::abs(chick_at(gameboy).x - eagle.x) <= kCellPx);

    run(gameboy, 4);
    const Chick lower = eagle_at(gameboy);
    REQUIRE(lower.found);
    REQUIRE(lower.y > eagle.y);
    REQUIRE(lower.x == eagle.x);

    bool killed = false;
    for (uint32_t frame = 0; frame < kSwoopFrames && !killed; ++frame) {
        gameboy.run_frame();
        killed = popup_shown(gameboy);
    }
    REQUIRE(killed);
    run(gameboy, 6);
    REQUIRE_FALSE(chick_at(gameboy).found);
    REQUIRE_FALSE(eagle_at(gameboy).found);
}

TEST_CASE("eagle_resets_on_progress") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    // four long idles, every one broken by a new lane: unreset, the timer would fire in the third
    for (int round = 0; round < 4; ++round) {
        // idle somewhere a plain hop leads on, so the round is a wait and not a river crossing
        for (int step = 0; step < 12; ++step) {
            const Chick c = chick_at(gameboy);
            REQUIRE(c.found);
            if (cell_tile(gameboy, c, 1, chick_cell(c)) == kGrassTileId) {
                break;
            }
            autopilot_step(gameboy);
        }

        const int before = hud_score(gameboy);
        run(gameboy, 200);
        REQUIRE_FALSE(eagle_at(gameboy).found);
        REQUIRE_FALSE(popup_shown(gameboy));

        tap(gameboy, gb::Button::Up);
        REQUIRE(hud_score(gameboy) == before + 1);
    }

    // well past a thousand frames of play and the eagle never had a reason to come
    REQUIRE_FALSE(eagle_at(gameboy).found);
    REQUIRE(chick_at(gameboy).found);
    REQUIRE_FALSE(popup_shown(gameboy));
}

TEST_CASE("camera_creeps") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    const Chick before = chick_at(gameboy);
    REQUIRE(before.found);
    const std::vector<int> grid_before = play_grid(gameboy);

    // four seconds of standing still and the camera has taken a lane on its own
    run(gameboy, 250);
    const Chick after = chick_at(gameboy);
    REQUIRE(after.found);
    REQUIRE(after.x == before.x);
    REQUIRE(after.y == before.y + kCellPx);

    // the same world, two tile rows further down the screen
    const std::vector<int> grid_after = play_grid(gameboy);
    size_t compared = 0;
    for (int cy = 2; cy < 18; ++cy) {
        for (int cx = 0; cx < 20; ++cx) {
            const int was = grid_before[static_cast<size_t>((cy - 2) * 20 + cx)];
            const int now = grid_after[static_cast<size_t>(cy * 20 + cx)];
            if (was == kNoTile || now == kNoTile) {
                continue;
            }
            REQUIRE(now == was);
            ++compared;
        }
    }
    REQUIRE(compared > 300u);
}

TEST_CASE("falling_behind_kills") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    const Chick home = chick_at(gameboy);
    REQUIRE(home.found);

    // two creeps of drift, then one lane of progress: enough to restart the idle timer, not
    // enough to catch the camera up, so the next creeps push the chick off the bottom first
    run(gameboy, 500);
    REQUIRE_FALSE(eagle_at(gameboy).found);
    const Chick drifted = chick_at(gameboy);
    REQUIRE(drifted.found);
    REQUIRE(drifted.y == home.y + 2 * kCellPx);

    tap(gameboy, gb::Button::Up);
    REQUIRE(hud_score(gameboy) == 1);

    bool swooped = false;
    bool killed = false;
    for (uint32_t frame = 0; frame < 700 && !killed; ++frame) {
        gameboy.run_frame();
        swooped = swooped || eagle_at(gameboy).found;
        killed = popup_shown(gameboy);
    }
    // the eagle took it, and well before the idle timer could have run out
    REQUIRE(swooped);
    REQUIRE(killed);
    run(gameboy, 6);
    REQUIRE_FALSE(chick_at(gameboy).found);
}

TEST_CASE("difficulty_ramps_car_speed") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    // fixed seeds whose deep road lanes roll fast; the opening ones cannot, whatever they roll
    // re-searched for milestone 6: the track roll from lane 15 shifts every later rng draw
    for (uint32_t hover_extra : {4u, 10u, 13u, 23u}) {
        gb::Gameboy gameboy;
        start_play(gameboy, rom, hover_extra);

        // lanes 0..6 are all the world holds yet, and every one of them is base tier
        const double base = fastest_car(gameboy);
        REQUIRE(base <= kBaseCarMax + kSpeedTol);

        double deep = 0;
        for (int step = 0; step < 60 && hud_score(gameboy) < 20; ++step) {
            if (!autopilot_step(gameboy)) {
                break;
            }
            // past lane 14 every visible lane was generated at 12 or beyond
            if (hud_score(gameboy) >= 14) {
                deep = std::max(deep, fastest_car(gameboy));
            }
        }
        REQUIRE(hud_score(gameboy) >= 14);
        REQUIRE(deep > kBaseCarMax + kSpeedTol);
    }
}

// the whole quiet-warning-sweep loop fits in this, well short of the eagle's patience
constexpr uint32_t kTrackWatchFrames = 400;
// the light first drops its crossbuck 15 frames into a 60 frame warning, so the sweep is 46 away
constexpr int kBlinkToTrainMin = 40;
constexpr int kBlinkToTrainMax = 55;
// 5 px a frame from just off the right edge takes this many frames to clear the screen
constexpr int kSweepMin = 30;
constexpr int kSweepMax = 46;
// the sweep enters at the right edge, so its first sighting reaches within a sprite of it
constexpr int kEnterX = 152;
// the two dings of a warning, kTrackBellGap apart
constexpr int kBellGap = 30;
constexpr int kBellSwing = 4000;

TEST_CASE("tracks_appear") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    // fixed seeds whose first track is the earliest one generation allows
    for (uint32_t hover_extra : {2u, 16u, 17u}) {
        gb::Gameboy gameboy;
        start_play(gameboy, rom, hover_extra);

        // nothing is generated past lane 14 until the camera reaches lane 9, so no rail can show yet
        for (int step = 0; step < 20 && hud_score(gameboy) < 7; ++step) {
            REQUIRE_FALSE(bg_has_tile(gameboy, kRailTileId));
            REQUIRE_FALSE(bg_has_tile(gameboy, kRailWarnTileId));
            REQUIRE(autopilot_step(gameboy));
        }

        REQUIRE(walk_to_track(gameboy, 60));
        REQUIRE(wait_for_quiet(gameboy, 1));
        const Chick c = chick_at(gameboy);
        REQUIRE(c.found);

        // rails end to end, and the lane the chick stands on is not one of them
        for (int col = 0; col < kGridCols; ++col) {
            REQUIRE(is_track_tile(cell_tile(gameboy, c, 1, col)));
        }
        REQUIRE_FALSE(is_track_tile(cell_tile(gameboy, c, 0, chick_cell(c))));
        // tracks come singly and a danger chunk is always followed by grass
        for (int col = 0; col < kGridCols; ++col) {
            REQUIRE_FALSE(is_track_tile(cell_tile(gameboy, c, 2, col)));
            REQUIRE_FALSE(is_water_tile(cell_tile(gameboy, c, 2, col)));
            REQUIRE_FALSE(is_road_tile(cell_tile(gameboy, c, 2, col)));
        }
        // one warning cell in the whole lane, near its center, and lit between sweeps
        REQUIRE(warn_cells_in_lane(gameboy, c, 1) == 1);
        REQUIRE(warn_cell(gameboy, c, 1) == kRailWarnTileId);
        REQUIRE(play_area_is_all_terrain(gameboy));
    }
}

TEST_CASE("warning_precedes_train") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    // fixed seeds whose first track is quiet on arrival, so the whole cycle is watched from the start
    for (uint32_t hover_extra : {6u, 7u, 10u, 34u}) {
        gb::Gameboy gameboy;
        start_play(gameboy, rom, hover_extra);
        REQUIRE(walk_to_track(gameboy, 60));
        REQUIRE(wait_for_quiet(gameboy, 1));

        int blink = -1;
        int train = -1;
        int gone = -1;
        int toggles = 0;
        int entered = -1;
        int leftmost = static_cast<int>(gb::kLcdWidth);
        bool off = false;
        bool leftward = true;
        for (uint32_t frame = 0; frame < kTrackWatchFrames && gone < 0; ++frame) {
            const Chick c = chick_at(gameboy);
            REQUIRE(c.found);
            const bool now_off = warn_cell(gameboy, c, 1) == kRailTileId;
            if (now_off != off) {
                off = now_off;
                ++toggles;
            }
            if (now_off && blink < 0) {
                blink = static_cast<int>(frame);
            }

            int x0 = -1;
            int x1 = -1;
            for (const CarRun& sweep : train_runs(gameboy)) {
                if (sweep.band == chick_band(c) - 1) {
                    x0 = sweep.x0;
                    x1 = sweep.x1;
                }
            }
            if (x0 >= 0) {
                if (train < 0) {
                    train = static_cast<int>(frame);
                    entered = x1;
                }
                leftward = leftward && x0 <= leftmost;
                leftmost = x0;
            } else if (train >= 0) {
                gone = static_cast<int>(frame);
            }
            gameboy.run_frame();
        }

        // no sweep ever came without the light blinking a whole warning ahead of it
        REQUIRE(blink >= 0);
        REQUIRE(train > blink);
        REQUIRE(train - blink >= kBlinkToTrainMin);
        REQUIRE(train - blink <= kBlinkToTrainMax);
        // the light swaps art four times over the warning, so at least three land before the train
        REQUIRE(toggles >= 3);

        // right to left: in at the screen's right edge, off at the left, never once back
        REQUIRE(entered >= kEnterX);
        REQUIRE(leftward);
        REQUIRE(leftmost == 0);
        REQUIRE(gone > train);
        REQUIRE(gone - train >= kSweepMin);
        REQUIRE(gone - train <= kSweepMax);
        REQUIRE_FALSE(popup_shown(gameboy));
    }
}

TEST_CASE("warning_rings_the_bell") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    for (uint32_t hover_extra : {6u, 34u}) {
        gb::Gameboy gameboy;
        start_play(gameboy, rom, hover_extra);
        REQUIRE(walk_to_track(gameboy, 60));
        REQUIRE(wait_for_quiet(gameboy, 1));
        drain_audio(gameboy);

        int first = -1;
        int second = -1;
        int blink = -1;
        int32_t peak = 0;
        bool ringing = false;
        for (uint32_t frame = 0; frame < kTrackWatchFrames; ++frame) {
            const Chick c = chick_at(gameboy);
            REQUIRE(c.found);
            if (blink < 0 && warn_cell(gameboy, c, 1) == kRailTileId) {
                blink = static_cast<int>(frame);
            }
            // the chick never moves here, so a swing this wide can only be the bell
            const int32_t swing = audio_swing(gameboy, 1);
            peak = std::max(peak, swing);
            // a ding decays over a dozen frames, so only its leading edge counts as a ring
            const bool loud = swing > kBellSwing;
            if (loud && !ringing) {
                if (first < 0) {
                    first = static_cast<int>(frame);
                } else {
                    second = static_cast<int>(frame);
                    break;
                }
            }
            ringing = loud;
        }
        REQUIRE(first >= 0);
        REQUIRE(second > 0);
        // rung at the warning's start and again halfway through it
        REQUIRE(second - first == kBellGap);
        REQUIRE(peak > kBellSwing);
        // and the first ding beats the first blink, which is a quarter of the warning in
        REQUIRE(blink > first);
    }
}

TEST_CASE("train_kills") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    // standing on the rails through a warning: the sweep takes the chick
    for (uint32_t hover_extra : {6u, 10u}) {
        gb::Gameboy gameboy;
        start_play(gameboy, rom, hover_extra);
        REQUIRE(walk_to_track(gameboy, 60));
        REQUIRE(wait_for_blink(gameboy, 1, kTrackWatchFrames));
        tap(gameboy, gb::Button::Up);
        REQUIRE_FALSE(popup_shown(gameboy));

        REQUIRE(wait_for_death(gameboy, 150));
        run(gameboy, 6);
        REQUIRE_FALSE(chick_at(gameboy).found);
        REQUIRE(train_runs(gameboy).empty());
    }

    // and hopping onto rails a sweep is already crossing is no better
    for (uint32_t hover_extra : {7u, 34u}) {
        gb::Gameboy gameboy;
        start_play(gameboy, rom, hover_extra);
        REQUIRE(walk_to_track(gameboy, 60));
        REQUIRE(wait_for_train(gameboy, 1, kTrackWatchFrames));
        tap(gameboy, gb::Button::Up);

        REQUIRE(wait_for_death(gameboy, 90));
        run(gameboy, 6);
        REQUIRE_FALSE(chick_at(gameboy).found);
    }
}

TEST_CASE("surviving_the_warning") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    for (uint32_t hover_extra : {6u, 10u}) {
        gb::Gameboy gameboy;
        start_play(gameboy, rom, hover_extra);
        REQUIRE(walk_to_track(gameboy, 60));
        REQUIRE(line_up_past_track(gameboy));
        const int score = hud_score(gameboy);

        // onto the rails with the light already blinking, held there, then off in time
        REQUIRE(wait_for_blink(gameboy, 1, kTrackWatchFrames));
        tap(gameboy, gb::Button::Up);
        REQUIRE(hud_score(gameboy) == score + 1);
        REQUIRE_FALSE(popup_shown(gameboy));

        run(gameboy, 15);
        REQUIRE_FALSE(popup_shown(gameboy));
        tap(gameboy, gb::Button::Up);
        REQUIRE(hud_score(gameboy) == score + 2);
        REQUIRE_FALSE(popup_shown(gameboy));

        // the danger was real: the sweep crossed the rails one lane back, the ones just left
        REQUIRE(wait_for_train(gameboy, -1, 90));
        REQUIRE_FALSE(popup_shown(gameboy));
        REQUIRE(chick_at(gameboy).found);
    }
}

TEST_CASE("autopilot_crosses_tracks") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    // fixed seeds whose deep world holds several tracks, crossed on a proven quiet light every time
    int total = 0;
    for (uint32_t hover_extra : {2u, 8u, 16u, 29u, 42u}) {
        gb::Gameboy gameboy;
        start_play(gameboy, rom, hover_extra);

        int tracks = 0;
        for (int step = 0; step < 80 && hud_score(gameboy) < 20; ++step) {
            const Chick c = chick_at(gameboy);
            REQUIRE(c.found);
            tracks += is_track_tile(cell_tile(gameboy, c, 1, chick_cell(c))) ? 1 : 0;
            if (!autopilot_step(gameboy)) {
                break;
            }
        }
        REQUIRE(tracks >= 1);
        REQUIRE(hud_score(gameboy) >= 20);
        REQUIRE(chick_at(gameboy).found);
        REQUIRE_FALSE(popup_shown(gameboy));
        REQUIRE(play_area_is_all_terrain(gameboy));
        total += tracks;
    }
    // not one lucky world: the five runs waited out several sweeps between them
    REQUIRE(total >= 6);
}

TEST_CASE("hop_makes_sound") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    // the start press is not a hop, so nothing has been triggered yet
    drain_audio(gameboy);

    const int32_t quiet = audio_swing(gameboy, 6);
    REQUIRE(quiet < kSilentSwing);
    gameboy.set_button(gb::Button::Up, true);
    gameboy.run_frame();
    gameboy.set_button(gb::Button::Up, false);
    REQUIRE(audio_swing(gameboy, 10) > kHopSwing);
}

TEST_CASE("death_makes_sound") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    for (int step = 0; step < 20; ++step) {
        const Chick c = chick_at(gameboy);
        REQUIRE(c.found);
        if (road_chunk_ahead(gameboy, c, chick_col(c)) > 0) {
            tap(gameboy, gb::Button::Up);
            break;
        }
        REQUIRE(autopilot_step(gameboy));
    }

    // the hop onto the asphalt has to decay before the crash can be measured against silence
    run(gameboy, 30);
    drain_audio(gameboy);
    REQUIRE(audio_swing(gameboy, 6) < kSilentSwing);

    int32_t burst = 0;
    bool died = false;
    for (uint32_t frame = 0; frame < 900 && !died; ++frame) {
        burst = std::max(burst, audio_swing(gameboy, 1));
        died = popup_shown(gameboy);
    }
    REQUIRE(died);
    // the car got the chick while it stood still, so the only sound in that stretch was the hit
    REQUIRE(burst > kDeathSwing);
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

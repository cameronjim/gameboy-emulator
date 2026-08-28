#include "terrain.h"

#include "assets.h"
#include "cars.h"
#include "crossy.h"

#include <gb/gb.h>
#include <stdint.h>

// sdcc has no cheap variable shift, so the column masks are a table
static const uint16_t kColBit[kGridCols] = {1U, 2U, 4U, 8U, 16U, 32U, 64U, 128U, 256U, 512U};

static uint8_t rng_state;
// lane records cached per ring slot, so a streamed row never re-rolls the rng
static uint16_t lane_trees[kRingLanes];
static uint8_t lane_road[kRingLanes];
// lanes come out in chunks; these carry the run of the chunk being generated
static uint8_t chunk_road;
static uint8_t chunk_left;
// guaranteed-open column of the last lane generated
static uint8_t prev_gap;
// next lane index to generate; always the camera lane plus the lanes ahead
static uint16_t gen_next;
static uint16_t cam_lane;
static uint8_t lane_buf[2U * kScreenCols];

static uint8_t rng_next(void) {
    rng_state = (uint8_t)(rng_state * kRngMul + kRngAdd);
    return rng_state;
}

// forward is up-screen, so map y falls as the lane index rises
static uint8_t ring_row(uint16_t lane) {
    return (uint8_t)((kMapRows - (uint8_t)((uint8_t)(lane & kRingLaneMask) << 1)) & (kMapRows - 1U));
}

// g wanders at most two columns per lane and never reaches an edge column
static uint8_t next_gap(void) {
    int8_t g = (int8_t)((int8_t)prev_gap + (int8_t)(rng_next() % kGapWanderSpan) - kGapWanderBias);
    if (g < kGapMin) {
        g = kGapMin;
    }
    if (g > kGapMax) {
        g = kGapMax;
    }
    return (uint8_t)g;
}

// the chunks alternate, so lanes 0..2 count as the run's opening grass chunk
static uint8_t next_kind(void) {
    if (chunk_left == 0U) {
        chunk_road = (uint8_t)(chunk_road ^ 1U);
        chunk_left = chunk_road ? (uint8_t)(kRoadChunkMin + rng_next() % kRoadChunkSpan)
                                : (uint8_t)(kGrassChunkMin + rng_next() % kGrassChunkSpan);
    }
    --chunk_left;
    return chunk_road;
}

static void generate_lane(uint16_t lane) {
    uint8_t slot = (uint8_t)(lane & kRingLaneMask);
    uint16_t trees = 0;
    uint8_t g = prev_gap;
    uint8_t road = 0;
    uint8_t lo;
    uint8_t hi;
    uint8_t n;
    uint8_t i;
    uint8_t c;

    if (lane != 0U) {
        g = next_gap();
    }
    lo = (g < prev_gap) ? g : prev_gap;
    hi = (g < prev_gap) ? prev_gap : g;

    if (lane >= kPlainLanes) {
        road = next_kind();
    }
    if (road != 0U) {
        // a road lane is always crossable ground; its danger is the traffic
        cars_lane_init(slot);
    } else if (lane >= kPlainLanes) {
        n = (uint8_t)(rng_next() % (kMaxTreesPerLane + 1U));
        for (i = 0; i < n; ++i) {
            c = (uint8_t)(rng_next() % kGridCols);
            // the span between the two gaps stays clear, so the path is always walkable
            if (c >= lo && c <= hi) {
                continue;
            }
            trees |= kColBit[c];
        }
    }

    prev_gap = g;
    lane_trees[slot] = trees;
    lane_road[slot] = road;
}

static void draw_lane(uint16_t lane) {
    uint8_t slot = (uint8_t)(lane & kRingLaneMask);
    uint16_t trees = lane_trees[slot];
    uint8_t c;
    uint8_t x;
    uint8_t t;

    for (c = 0; c < kGridCols; ++c) {
        x = (uint8_t)(c << 1);
        if (lane_road[slot] != 0U) {
            // one dash per cell: the stripe tile carries the center line's bottom rows
            lane_buf[x] = kRoadStripeTileId;
            lane_buf[x + 1U] = kRoadTileId;
            lane_buf[kScreenCols + x] = kRoadTileId;
            lane_buf[kScreenCols + x + 1U] = kRoadTileId;
            continue;
        }
        t = (trees & kColBit[c]) != 0U ? kTreeTileId : kGrassTileId;
        lane_buf[x] = t;
        lane_buf[x + 1U] = t;
        lane_buf[kScreenCols + x] = t;
        lane_buf[kScreenCols + x + 1U] = t;
    }
    set_bkg_tiles(0, ring_row(lane), kScreenCols, 2, lane_buf);
}

// a fresh ring holds nothing but grass, so no cell can ever show a stale tile
static void fill_ring_with_grass(void) {
    uint8_t i;

    for (i = 0; i < 2U * kScreenCols; ++i) {
        lane_buf[i] = kGrassTileId;
    }
    for (i = 0; i < kRingLanes; ++i) {
        set_bkg_tiles(0, (uint8_t)(i << 1), kScreenCols, 2, lane_buf);
    }
}

void terrain_init(uint8_t seed) {
    uint8_t i;

    // seeded from the hover frame count, so scripted runs stay stable
    rng_state = seed;
    prev_gap = kChickSpawnCol;
    cam_lane = 0;
    gen_next = 0;
    chunk_road = 0;
    chunk_left = 0;

    set_bkg_data(kGrassTileId, 1, kGrassTile);
    set_bkg_data(kTreeTileId, 1, kTreeTile);
    set_bkg_data(kRoadTileId, 1, kRoadTile);
    set_bkg_data(kRoadStripeTileId, 1, kRoadStripeTile);

    for (i = 0; i < kRingLanes; ++i) {
        lane_trees[i] = 0;
        lane_road[i] = 0;
    }

    SCX_REG = 0;
    fill_ring_with_grass();
    for (i = 0; i < kInitLanes; ++i) {
        generate_lane(gen_next);
        draw_lane(gen_next);
        ++gen_next;
    }
    terrain_apply_scy(0);
}

uint8_t terrain_blocked(uint16_t lane, uint8_t col) {
    return (lane_trees[lane & kRingLaneMask] & kColBit[col]) != 0U ? 1U : 0U;
}

uint8_t terrain_is_road(uint16_t lane) {
    return lane_road[lane & kRingLaneMask];
}

uint8_t terrain_lane_screen_y(uint16_t lane) {
    return (uint8_t)((uint8_t)(ring_row(lane) << 3) - SCY_REG);
}

uint16_t terrain_cam_lane(void) {
    return cam_lane;
}

void terrain_advance(void) {
    ++cam_lane;
    // the new lane sits one row above the top edge until scy slides, so it is safe to write
    generate_lane(gen_next);
    draw_lane(gen_next);
    ++gen_next;
}

void terrain_apply_scy(uint8_t slide_px) {
    uint8_t top = (uint8_t)((uint8_t)(cam_lane & kRingLaneMask) << 4);
    SCY_REG = (uint8_t)((uint8_t)(0U - top) - kCamLaneScreenY + slide_px);
}

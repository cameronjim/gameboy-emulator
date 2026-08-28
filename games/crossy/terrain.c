#include "terrain.h"

#include "assets.h"
#include "crossy.h"
#include "movers.h"

#include <gb/gb.h>
#include <stdint.h>

// sdcc has no cheap variable shift, so the column masks are a table
static const uint16_t kColBit[kGridCols] = {1U, 2U, 4U, 8U, 16U, 32U, 64U, 128U, 256U, 512U};

// the difficulty ramp's own two columns; the mover speeds live with the movers
static const uint16_t kRampLane[kRampTiers] = kRampLaneList;
static const uint8_t kRampTrees[kRampTiers] = kRampTreesList;

static uint8_t rng_state;
// lane records cached per ring slot, so a streamed row never re-rolls the rng
static uint16_t lane_trees[kRingLanes];
static uint8_t lane_kind[kRingLanes];
// 1 when a road lane borders another road lane of its chunk below it, so it draws the dash row
static uint8_t lane_dash[kRingLanes];
// a track lane's phase machine, ticked only while the lane is on screen
static uint8_t track_phase[kRingLanes];
static uint16_t track_timer[kRingLanes];
static int16_t train_x[kRingLanes];
// the warning cell's current art, so vram is written only on a blink
static uint8_t warn_shown[kRingLanes];
// lanes come out in chunks; these carry the run of the chunk being generated
static uint8_t chunk_kind;
static uint8_t chunk_left;
// guaranteed-open column of the last lane generated
static uint8_t prev_gap;
// next lane index to generate; always the camera lane plus the lanes ahead
static uint16_t gen_next;
static uint16_t cam_lane;
// play frames since the camera last moved; the creep is what punishes standing still
static uint16_t creep_frames;
static uint8_t lane_buf[2U * kScreenCols];

static uint8_t rng_next(void) {
    rng_state = (uint8_t)(rng_state * kRngMul + kRngAdd);
    return rng_state;
}

// forward is up-screen, so map y falls as the lane index rises
static uint8_t ring_row(uint16_t lane) {
    return (uint8_t)((kMapRows - (uint8_t)((uint8_t)(lane & kRingLaneMask) << 1)) & (kMapRows - 1U));
}

static uint8_t ramp_tier(uint16_t lane) {
    uint8_t t = 0;

    while ((uint8_t)(t + 1U) < kRampTiers && lane >= kRampLane[t + 1U]) {
        ++t;
    }
    return t;
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

// grass and danger chunks alternate, so lanes 0..2 count as the run's opening grass chunk
static uint8_t next_kind(uint16_t lane) {
    if (chunk_left == 0U) {
        if (chunk_kind != kLaneGrass) {
            chunk_kind = kLaneGrass;
            chunk_left = (uint8_t)(kGrassChunkMin + rng_next() % kGrassChunkSpan);
        } else if (lane >= kTrackFirstLane && rng_next() % kTrackOdds == 0U) {
            chunk_kind = kLaneTrack;
            chunk_left = kTrackChunkLanes;
        } else if ((rng_next() & kRngTopBit) != 0U) {
            // this lcg's low bit flips every call, so the coin toss reads its top one
            chunk_kind = kLaneWater;
            chunk_left = (uint8_t)(kWaterChunkMin + (uint8_t)(rng_next() >> 4) % kWaterChunkSpan);
        } else {
            chunk_kind = kLaneRoad;
            chunk_left = (uint8_t)(kRoadChunkMin + rng_next() % kRoadChunkSpan);
        }
    }
    --chunk_left;
    return chunk_kind;
}

static uint16_t quiet_frames(void) {
    return (uint16_t)(kTrackQuietMin + (uint16_t)(rng_next() % kTrackQuietSpan));
}

static void track_init(uint8_t slot) {
    track_phase[slot] = kTrackQuiet;
    track_timer[slot] = quiet_frames();
    train_x[slot] = kTrainOffX;
    warn_shown[slot] = kRailWarnTileId;
}

static void generate_lane(uint16_t lane) {
    uint8_t slot = (uint8_t)(lane & kRingLaneMask);
    uint8_t tier = ramp_tier(lane);
    uint16_t trees = 0;
    uint8_t g = prev_gap;
    uint8_t kind = kLaneGrass;
    uint8_t chained;
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
        kind = next_kind(lane);
    }
    chained = (lane != 0U && lane_kind[(uint8_t)(lane - 1U) & kRingLaneMask] == kind) ? 1U : 0U;
    if (kind == kLaneTrack) {
        track_init(slot);
    } else if (kind != kLaneGrass) {
        // asphalt and open water are both clear ground; their danger is what slides along them
        movers_lane_init(slot, (uint8_t)(kind == kLaneWater), tier, chained);
    } else if (lane >= kPlainLanes) {
        n = (uint8_t)(rng_next() % (uint8_t)(kRampTrees[tier] + 1U));
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
    lane_kind[slot] = kind;
    // a danger chunk is always followed by grass, so an adjacent road lane is always the same chunk
    lane_dash[slot] = (uint8_t)(kind == kLaneRoad && chained != 0U);
}

static void draw_lane(uint16_t lane) {
    uint8_t slot = (uint8_t)(lane & kRingLaneMask);
    uint16_t trees = lane_trees[slot];
    uint8_t c;
    uint8_t x;
    uint8_t t;

    if (lane_kind[slot] == kLaneTrack) {
        for (c = 0; c < 2U * kScreenCols; ++c) {
            lane_buf[c] = kRailTileId;
        }
        lane_buf[kTrackWarnCol] = kRailWarnTileId;
        set_bkg_tiles(0, ring_row(lane), kScreenCols, 2, lane_buf);
        return;
    }
    for (c = 0; c < kGridCols; ++c) {
        x = (uint8_t)(c << 1);
        if (lane_kind[slot] == kLaneRoad) {
            // the dash row marks the seam with the road lane below; an outer edge is plain asphalt
            lane_buf[x] = kRoadTileId;
            lane_buf[x + 1U] = kRoadTileId;
            lane_buf[kScreenCols + x] = lane_dash[slot] != 0U ? kRoadStripeTileId : kRoadTileId;
            lane_buf[kScreenCols + x + 1U] = kRoadTileId;
            continue;
        }
        if (lane_kind[slot] == kLaneWater) {
            // glints on the top row, calm below, so the 16 px lane reads as one band
            lane_buf[x] = kWaterTileId;
            lane_buf[x + 1U] = kWaterTileId;
            lane_buf[kScreenCols + x] = kWaterCalmTileId;
            lane_buf[kScreenCols + x + 1U] = kWaterCalmTileId;
            continue;
        }
        if ((trees & kColBit[c]) != 0U) {
            t = kTreeTileId;
        } else {
            // even lanes take one grass tile and odd lanes the other, so every boundary is drawn
            t = (lane & 1U) != 0U ? kGrassAltTileId : kGrassTileId;
        }
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
    uint8_t c;

    for (i = 0; i < kRingLanes; ++i) {
        // a slot's parity is its lane's, so the fill alternates exactly as the real lanes will
        for (c = 0; c < 2U * kScreenCols; ++c) {
            lane_buf[c] = (i & 1U) != 0U ? kGrassAltTileId : kGrassTileId;
        }
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
    creep_frames = 0;
    chunk_kind = kLaneGrass;
    chunk_left = 0;

    set_bkg_data(kGrassTileId, 1, kGrassTile);
    set_bkg_data(kGrassAltTileId, 1, kGrassAltTile);
    set_bkg_data(kTreeTileId, 1, kTreeTile);
    set_bkg_data(kRoadTileId, 1, kRoadTile);
    set_bkg_data(kRoadStripeTileId, 1, kRoadStripeTile);
    set_bkg_data(kWaterTileId, 1, kWaterTile);
    set_bkg_data(kWaterCalmTileId, 1, kWaterCalmTile);

    set_bkg_data(kRailTileId, 1, kRailTile);
    set_bkg_data(kRailWarnTileId, 1, kRailWarnTile);

    for (i = 0; i < kRingLanes; ++i) {
        lane_trees[i] = 0;
        lane_kind[i] = kLaneGrass;
        lane_dash[i] = 0;
        track_phase[i] = kTrackQuiet;
        track_timer[i] = kTrackQuietMin;
        train_x[i] = kTrainOffX;
        warn_shown[i] = kRailWarnTileId;
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

void terrain_redraw_lane(uint16_t lane) {
    draw_lane(lane);
}

uint8_t terrain_blocked(uint16_t lane, uint8_t col) {
    return (lane_trees[lane & kRingLaneMask] & kColBit[col]) != 0U ? 1U : 0U;
}

uint8_t terrain_is_road(uint16_t lane) {
    return lane_kind[lane & kRingLaneMask] == kLaneRoad ? 1U : 0U;
}

uint8_t terrain_is_water(uint16_t lane) {
    return lane_kind[lane & kRingLaneMask] == kLaneWater ? 1U : 0U;
}

uint8_t terrain_is_track(uint16_t lane) {
    return lane_kind[lane & kRingLaneMask] == kLaneTrack ? 1U : 0U;
}

int16_t terrain_train_x(uint16_t lane) {
    uint8_t slot = (uint8_t)(lane & kRingLaneMask);

    if (lane_kind[slot] != kLaneTrack || track_phase[slot] != kTrackTrain) {
        return kTrainOffX;
    }
    return train_x[slot];
}

uint8_t terrain_train_hit(uint16_t lane, uint8_t center_x) {
    int16_t x = terrain_train_x(lane);
    int16_t d;

    if (x == kTrainOffX) {
        return 0;
    }
    d = (int16_t)((int16_t)center_x - x);
    return (d > -(int16_t)kChickHalfPx && d < (int16_t)(kTrainPx + kChickHalfPx)) ? 1U : 0U;
}

// the light holds its crossbuck except on the blink's off half; only a warning ever shows bare rail
static uint8_t warn_art(uint8_t slot) {
    uint8_t elapsed;

    if (track_phase[slot] != kTrackWarn) {
        return kRailWarnTileId;
    }
    elapsed = (uint8_t)(kTrackWarnFrames - track_timer[slot]);
    return ((uint8_t)(elapsed / kTrackBlinkFrames) & 1U) != 0U ? kRailTileId : kRailWarnTileId;
}

// one lane's frame of the quiet -> warning -> train loop; reads 1 on the frames a bell is due
static uint8_t tick_track(uint16_t lane) {
    uint8_t slot = (uint8_t)(lane & kRingLaneMask);
    uint8_t bell = 0;
    uint8_t art;

    if (track_phase[slot] == kTrackQuiet) {
        --track_timer[slot];
        if (track_timer[slot] == 0U) {
            track_phase[slot] = kTrackWarn;
            track_timer[slot] = kTrackWarnFrames;
            bell = 1;
        }
    } else if (track_phase[slot] == kTrackWarn) {
        --track_timer[slot];
        if (track_timer[slot] == kTrackWarnFrames - kTrackBellGap) {
            bell = 1;
        }
        if (track_timer[slot] == 0U) {
            track_phase[slot] = kTrackTrain;
            train_x[slot] = kTrainStartX;
        }
    } else {
        train_x[slot] = (int16_t)(train_x[slot] - kTrainSpeedPx);
        if (train_x[slot] <= -(int16_t)kTrainPx) {
            track_phase[slot] = kTrackQuiet;
            track_timer[slot] = quiet_frames();
            train_x[slot] = kTrainOffX;
        }
    }

    art = warn_art(slot);
    if (art != warn_shown[slot]) {
        warn_shown[slot] = art;
        set_bkg_tiles(kTrackWarnCol, ring_row(lane), 1, 1, &warn_shown[slot]);
    }
    return bell;
}

uint8_t terrain_tick_tracks(void) {
    uint16_t lane = (cam_lane > kMaxLanesBehind) ? (uint16_t)(cam_lane - kMaxLanesBehind) : 0U;
    uint16_t last = (uint16_t)(cam_lane + kLanesAhead);
    uint8_t bell = 0;

    for (; lane <= last; ++lane) {
        if (lane_kind[lane & kRingLaneMask] == kLaneTrack) {
            bell = (uint8_t)(bell | tick_track(lane));
        }
    }
    return bell;
}

uint8_t terrain_lane_screen_y(uint16_t lane) {
    return (uint8_t)((uint8_t)(ring_row(lane) << 3) - SCY_REG);
}

uint16_t terrain_cam_lane(void) {
    return cam_lane;
}

void terrain_advance(void) {
    ++cam_lane;
    creep_frames = 0;
    // the new lane sits one row above the top edge until scy slides, so it is safe to write
    generate_lane(gen_next);
    draw_lane(gen_next);
    ++gen_next;
}

uint8_t terrain_creep_due(void) {
    if (creep_frames < kCreepFrames) {
        ++creep_frames;
    }
    // it saturates, so a creep landing mid slide simply waits for the next settled frame
    return creep_frames >= kCreepFrames ? 1U : 0U;
}

void terrain_apply_scy(uint8_t slide_px) {
    uint8_t top = (uint8_t)((uint8_t)(cam_lane & kRingLaneMask) << 4);
    SCY_REG = (uint8_t)((uint8_t)(0U - top) - kCamLaneScreenY + slide_px);
}

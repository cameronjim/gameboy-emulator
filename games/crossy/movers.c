#include "movers.h"

#include "assets.h"
#include "crossy.h"
#include "terrain.h"

#include <gb/gb.h>
#include <stdint.h>

// track records cached per ring slot, alongside terrain's lane records
static uint8_t rng_state;
static uint8_t mover_dir[kRingLanes];
static uint16_t mover_speed[kRingLanes];
// 8.8 track position of the lane's first mover; the second trails half a lap behind
static uint16_t mover_pos[kRingLanes];

static uint8_t rng_next(void) {
    rng_state = (uint8_t)(rng_state * kRngMul + kRngAdd);
    return rng_state;
}

// track x doubles as oam x, so a mover spans screen x-half to x+half and centers on x
static uint8_t track_x(uint8_t slot, uint8_t which) {
    uint16_t p = mover_pos[slot];

    if (which != 0U) {
        p = (uint16_t)(p + kMoverPhase);
    }
    return (uint8_t)(p >> 8);
}

// oam y 0 parks a sprite entirely above the screen
static void park(uint8_t sprite) {
    move_sprite(sprite, 0, 0);
}

static int16_t lane_step(uint8_t slot) {
    return mover_dir[slot] != 0U ? (int16_t)mover_speed[slot] : (int16_t)(0 - (int16_t)mover_speed[slot]);
}

// a mover is a row of 8x8 sprites centered on the track x, parked whole once the track leaves screen
static uint8_t draw_mover(uint8_t sprite, uint8_t tx, uint8_t y, uint8_t water) {
    uint8_t parts = water ? kLogSprites : kCarSprites;
    uint8_t x = (uint8_t)((uint8_t)(tx - (water ? kLogHalfPx : kCarHalfPx)) + kOamXOffset);
    uint8_t i;

    for (i = 0; i < parts; ++i) {
        if (tx >= kMoverDrawLimit) {
            park(sprite);
        } else {
            // a car's two halves are consecutive tiles; a log repeats the one tile
            set_sprite_tile(sprite, water ? kLogTileId : (uint8_t)(kCarTileId + i));
            move_sprite(sprite, x, y);
        }
        x = (uint8_t)(x + kSpritePx);
        ++sprite;
    }
    return sprite;
}

void movers_init(uint8_t seed) {
    uint8_t i;

    rng_state = seed;
    set_sprite_data(kCarTileId, kCarTileCount, kCarTiles);
    set_sprite_data(kLogTileId, kLogTileCount, kLogTile);
    for (i = 0; i < kMoverSprites; ++i) {
        set_sprite_prop((uint8_t)(kMoverFirstSprite + i), 0);
        park((uint8_t)(kMoverFirstSprite + i));
    }
    for (i = 0; i < kRingLanes; ++i) {
        mover_dir[i] = 0;
        mover_speed[i] = kCarSpeedMin;
        mover_pos[i] = 0;
    }
}

void movers_lane_init(uint8_t slot, uint8_t water) {
    uint8_t roll = rng_next();
    uint8_t pick = (uint8_t)(roll >> 1);

    mover_dir[slot] = (uint8_t)(roll & 1U);
    if (water != 0U) {
        mover_speed[slot] = (uint16_t)(kLogSpeedMin + (uint16_t)(pick % kLogSpeedSteps) * kLogSpeedStep);
    } else {
        mover_speed[slot] = (uint16_t)(kCarSpeedMin + (uint16_t)(pick % kCarSpeedSteps) * kCarSpeedStep);
    }
    mover_pos[slot] = (uint16_t)((uint16_t)rng_next() << 8);
}

void movers_hide(void) {
    uint8_t i;

    for (i = 0; i < kMoverSprites; ++i) {
        park((uint8_t)(kMoverFirstSprite + i));
    }
}

void movers_update(void) {
    uint16_t cam = terrain_cam_lane();
    uint16_t lane = (cam > kMaxLanesBehind) ? (uint16_t)(cam - kMaxLanesBehind) : 0U;
    uint16_t last = (uint16_t)(cam + kLanesAhead);
    uint8_t sprite = kMoverFirstSprite;
    uint8_t pool_end = (uint8_t)(kMoverFirstSprite + kMoverSprites);
    uint8_t water;
    uint8_t which;
    uint8_t slot;
    uint8_t y;

    for (; lane <= last; ++lane) {
        water = terrain_is_water(lane);
        if (water == 0U && terrain_is_road(lane) == 0U) {
            continue;
        }
        // generation caps the visible danger lanes, so the pool never runs dry
        if ((uint8_t)(sprite + (uint8_t)(kMoversPerLane * (water ? kLogSprites : kCarSprites))) > pool_end) {
            break;
        }
        slot = (uint8_t)(lane & kRingLaneMask);
        mover_pos[slot] = (uint16_t)(mover_pos[slot] + (uint16_t)lane_step(slot));
        // read back from scy so the movers ride the lane through a hop's slide
        y = (uint8_t)(terrain_lane_screen_y(lane) + (water ? kLogLaneInset : kMoverLaneInset) + kOamYOffset);
        for (which = 0; which < kMoversPerLane; ++which) {
            sprite = draw_mover(sprite, track_x(slot, which), y, water);
        }
    }
    for (; sprite < pool_end; ++sprite) {
        park(sprite);
    }
}

uint8_t movers_car_hit(uint16_t lane, uint8_t center_x) {
    uint8_t slot = (uint8_t)(lane & kRingLaneMask);
    uint8_t which;
    int16_t d;

    if (terrain_is_road(lane) == 0U) {
        return 0;
    }
    for (which = 0; which < kMoversPerLane; ++which) {
        d = (int16_t)((int16_t)track_x(slot, which) - (int16_t)center_x);
        if (d > -kCarHitPx && d < kCarHitPx) {
            return 1;
        }
    }
    return 0;
}

uint8_t movers_log_ride(uint16_t lane, uint8_t center_x, int16_t* step) {
    uint8_t slot = (uint8_t)(lane & kRingLaneMask);
    uint8_t which;
    int16_t d;

    if (terrain_is_water(lane) == 0U) {
        return 0;
    }
    for (which = 0; which < kMoversPerLane; ++which) {
        d = (int16_t)((int16_t)track_x(slot, which) - (int16_t)center_x);
        if (d >= -kLogRidePx && d <= kLogRidePx) {
            // the same 8.8 add the log took this frame, so the ride never drifts
            *step = lane_step(slot);
            return 1;
        }
    }
    return 0;
}

#include "cars.h"

#include "assets.h"
#include "crossy.h"
#include "terrain.h"

#include <gb/gb.h>
#include <stdint.h>

// car records cached per ring slot, alongside terrain's lane records
static uint8_t rng_state;
static uint8_t car_dir[kRingLanes];
static uint16_t car_speed[kRingLanes];
// 8.8 track position of the lane's first car; the second trails half a lap behind
static uint16_t car_pos[kRingLanes];

static uint8_t rng_next(void) {
    rng_state = (uint8_t)(rng_state * kRngMul + kRngAdd);
    return rng_state;
}

// track x doubles as oam x, so a car spans screen x-8 to x+8 and centers on x
static uint8_t track_x(uint8_t slot, uint8_t which) {
    uint16_t p = car_pos[slot];

    if (which != 0U) {
        p = (uint16_t)(p + kCarPhase);
    }
    return (uint8_t)(p >> 8);
}

// oam y 0 parks a sprite entirely above the screen
static void park(uint8_t sprite) {
    move_sprite(sprite, 0, 0);
}

void cars_init(uint8_t seed) {
    uint8_t i;

    rng_state = seed;
    set_sprite_data(kCarTileId, kCarTileCount, kCarTiles);
    for (i = 0; i < kCarSprites; ++i) {
        // the pool alternates front and rear halves, so a draw only ever moves sprites
        set_sprite_tile((uint8_t)(kCarFirstSprite + i), (uint8_t)(kCarTileId + (uint8_t)(i & 1U)));
        set_sprite_prop((uint8_t)(kCarFirstSprite + i), 0);
        park((uint8_t)(kCarFirstSprite + i));
    }
    for (i = 0; i < kRingLanes; ++i) {
        car_dir[i] = 0;
        car_speed[i] = kCarSpeedMin;
        car_pos[i] = 0;
    }
}

void cars_lane_init(uint8_t slot) {
    uint8_t roll = rng_next();

    car_dir[slot] = (uint8_t)(roll & 1U);
    car_speed[slot] =
        (uint16_t)(kCarSpeedMin + (uint16_t)((uint8_t)(roll >> 1) % kCarSpeedSteps) * kCarSpeedStep);
    car_pos[slot] = (uint16_t)((uint16_t)rng_next() << 8);
}

void cars_hide(void) {
    uint8_t i;

    for (i = 0; i < kCarSprites; ++i) {
        park((uint8_t)(kCarFirstSprite + i));
    }
}

void cars_update(void) {
    uint16_t cam = terrain_cam_lane();
    uint16_t lane = (cam > kMaxLanesBehind) ? (uint16_t)(cam - kMaxLanesBehind) : 0U;
    uint16_t last = (uint16_t)(cam + kLanesAhead);
    uint8_t sprite = kCarFirstSprite;
    uint8_t slot;
    uint8_t which;
    uint8_t tx;
    uint8_t y;

    for (; lane <= last; ++lane) {
        if (!terrain_is_road(lane)) {
            continue;
        }
        // generation caps the visible road lanes at six, so the pool never runs dry
        if (sprite > (uint8_t)(kCarFirstSprite + kCarSprites - kCarSpritesPerLane)) {
            break;
        }
        slot = (uint8_t)(lane & kRingLaneMask);
        if (car_dir[slot] != 0U) {
            car_pos[slot] = (uint16_t)(car_pos[slot] + car_speed[slot]);
        } else {
            car_pos[slot] = (uint16_t)(car_pos[slot] - car_speed[slot]);
        }
        // read back from scy so the cars ride the lane through a hop's slide
        y = (uint8_t)(terrain_lane_screen_y(lane) + kCarLaneInset + kOamYOffset);
        for (which = 0; which < kCarsPerLane; ++which) {
            tx = track_x(slot, which);
            if (tx < kCarDrawLimit) {
                move_sprite(sprite, tx, y);
                move_sprite((uint8_t)(sprite + 1U), (uint8_t)(tx + kCarHalfPx), y);
            } else {
                park(sprite);
                park((uint8_t)(sprite + 1U));
            }
            sprite = (uint8_t)(sprite + 2U);
        }
    }
    for (; sprite < (uint8_t)(kCarFirstSprite + kCarSprites); ++sprite) {
        park(sprite);
    }
}

uint8_t cars_hit(uint16_t lane, uint8_t center_x) {
    uint8_t slot = (uint8_t)(lane & kRingLaneMask);
    uint8_t which;
    int16_t d;

    if (!terrain_is_road(lane)) {
        return 0;
    }
    for (which = 0; which < kCarsPerLane; ++which) {
        d = (int16_t)((int16_t)track_x(slot, which) - (int16_t)center_x);
        if (d > -kCarHitPx && d < kCarHitPx) {
            return 1;
        }
    }
    return 0;
}

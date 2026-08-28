#include "eagle.h"

#include "assets.h"
#include "crossy.h"

#include <gb/gb.h>
#include <stdint.h>

static uint16_t idle;
static uint8_t diving;
static uint8_t summoned;
// locked at the strike, so the bird falls straight down the column the chick was standing in
static uint8_t dive_x;
static uint8_t dive_y;

// oam y 0 parks a sprite entirely above the screen
static void park(uint8_t sprite) {
    move_sprite(sprite, 0, 0);
}

static void draw(void) {
    uint8_t i;

    for (i = 0; i < kEagleSprites; ++i) {
        move_sprite((uint8_t)(kEagleFirstSprite + i),
                    (uint8_t)((uint8_t)(dive_x - kEagleHalfPx) + kOamXOffset + (uint8_t)(i * kSpritePx)),
                    (uint8_t)(dive_y + kOamYOffset));
    }
}

void eagle_init(void) {
    uint8_t i;

    idle = 0;
    diving = 0;
    summoned = 0;
    dive_x = 0;
    dive_y = kEagleStartY;
    set_sprite_data(kEagleTileId, kEagleTileCount, kEagleTiles);
    for (i = 0; i < kEagleSprites; ++i) {
        set_sprite_prop((uint8_t)(kEagleFirstSprite + i), 0);
        set_sprite_tile((uint8_t)(kEagleFirstSprite + i),
                        (uint8_t)(kEagleTileId + (uint8_t)(i * kTilesPerSprite)));
        park((uint8_t)(kEagleFirstSprite + i));
    }
}

void eagle_reset(void) {
    idle = 0;
}

void eagle_summon(void) {
    summoned = 1;
}

uint8_t eagle_active(void) {
    return diving;
}

uint8_t eagle_update(uint8_t chick_x, uint8_t chick_y) {
    if (diving == 0U) {
        if (summoned == 0U) {
            if (idle < kEagleIdleFrames) {
                ++idle;
            }
            if (idle < kEagleIdleFrames) {
                return 0;
            }
        }
        summoned = 0;
        diving = 1;
        dive_x = chick_x;
        dive_y = kEagleStartY;
    } else {
        dive_y = (uint8_t)(dive_y + kEagleDivePx);
    }
    draw();
    return dive_y >= chick_y ? 1U : 0U;
}

void eagle_hide(void) {
    uint8_t i;

    for (i = 0; i < kEagleSprites; ++i) {
        park((uint8_t)(kEagleFirstSprite + i));
    }
}

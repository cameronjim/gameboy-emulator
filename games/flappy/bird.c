#include "bird.h"

#include "assets.h"
#include "flappy.h"

#include <gb/gb.h>
#include <stdint.h>

// 8.8 fixed point, measured from the top of the playfield
static int16_t bird_y;
static int16_t bird_vy;

void bird_init(void) {
    bird_y = kBirdStartY;
    bird_vy = 0;
    OBP0_REG = kBirdObp;
    set_sprite_data(kBirdTileId, 1, kBirdTile);
    set_sprite_tile(0, kBirdTileId);
    bird_draw();
}

void bird_flap(void) {
    bird_vy = kFlapVy;
}

void bird_update(void) {
    bird_vy += kGravityVy;
    if (bird_vy > kTerminalVy) {
        bird_vy = kTerminalVy;
    }
    bird_y += bird_vy;
    if (bird_y < kBirdCeilingY) {
        bird_y = kBirdCeilingY;
        bird_vy = 0;
    }
    // resting the bird on the ground: world_kills ends the round the same frame
    if (bird_y > kBirdFloorY) {
        bird_y = kBirdFloorY;
        bird_vy = 0;
    }
}

uint8_t bird_top_px(void) {
    return (uint8_t)(bird_y >> kFixedShift);
}

void bird_draw(void) {
    move_sprite(0, kBirdOamX, (uint8_t)((bird_y >> kFixedShift) + kBirdOamYOffset));
}

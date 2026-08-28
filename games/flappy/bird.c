#include "bird.h"

#include "assets.h"
#include "flappy.h"

#include <gb/gb.h>
#include <stdint.h>

// 8.8 fixed point, measured from the top of the playfield
static int16_t bird_y;
static int16_t bird_vy;
// frames left in the flap animation; 0 rests on the glide frame
static uint8_t anim;
static uint8_t bob;

// title wing loop: up, glide, down, glide
static const uint8_t kTitleFrames[4] = {kBirdFrameUp, kBirdFrameGlide, kBirdFrameDown, kBirdFrameGlide};

static uint8_t frame_tile(void) {
    if (anim > 2U * kFlapAnimStep) {
        return kBirdFrameUp;
    }
    if (anim > kFlapAnimStep) {
        return kBirdFrameGlide;
    }
    if (anim > 0U) {
        return kBirdFrameDown;
    }
    return kBirdFrameGlide;
}

void bird_init(void) {
    bird_y = kBirdStartY;
    bird_vy = 0;
    anim = 0;
    bob = 0;
    OBP0_REG = kBirdObp;
    set_sprite_data(kBirdTileId, kBirdFrames, kBirdTiles);
    set_sprite_prop(0, 0);
    bird_draw();
}

void bird_flap(void) {
    bird_vy = kFlapVy;
    anim = kFlapAnimFrames;
}

void bird_update(void) {
    if (anim > 0U) {
        --anim;
    }
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

void bird_hover(void) {
    uint8_t tri;

    tri = (bob < kTitleBobHalf) ? bob : (uint8_t)(kTitleBobPeriod - bob);
    bird_y = (int16_t)(kBirdStartY - kTitleBobBias + (int16_t)tri * kTitleBobStep);
    move_sprite(0, kBirdOamX, (uint8_t)((bird_y >> kFixedShift) + kBirdOamYOffset));
    set_sprite_tile(0, (uint8_t)(kBirdTileId + kTitleFrames[(bob >> kTitleFlapShift) & 3U]));
    ++bob;
    if (bob >= kTitleBobPeriod) {
        bob = 0;
    }
}

// the popup can cover wherever the bird died, so park it clear of the screen
void bird_hide(void) {
    move_sprite(0, 0, 0);
}

uint8_t bird_top_px(void) {
    return (uint8_t)(bird_y >> kFixedShift);
}

void bird_draw(void) {
    move_sprite(0, kBirdOamX, (uint8_t)((bird_y >> kFixedShift) + kBirdOamYOffset));
    set_sprite_tile(0, (uint8_t)(kBirdTileId + frame_tile()));
}

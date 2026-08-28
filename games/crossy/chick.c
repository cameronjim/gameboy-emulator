#include "chick.h"

#include "assets.h"
#include "crossy.h"
#include "terrain.h"

#include <gb/gb.h>
#include <stdint.h>

static uint16_t lane;
static uint8_t col;
// px still owed on the current hop; only one of the three is ever nonzero
static int8_t slide_x;
static int8_t slide_y;
static uint8_t slide_scy;

static uint8_t hopping(void) {
    return (slide_x != 0 || slide_y != 0 || slide_scy != 0U) ? 1U : 0U;
}

static int8_t step_toward_zero(int8_t v) {
    if (v > 0) {
        return (int8_t)(v - kHopStepPx);
    }
    if (v < 0) {
        return (int8_t)(v + kHopStepPx);
    }
    return 0;
}

static void hop_forward(void) {
    uint16_t next = (uint16_t)(lane + 1U);

    if (terrain_blocked(next, col)) {
        return;
    }
    lane = next;
    if (next > terrain_cam_lane()) {
        // the camera keeps pace, so the chick holds its screen row while the world slides
        terrain_advance();
        slide_scy = kHopSlidePx;
    } else {
        slide_y = kHopSlidePx;
    }
}

static void hop_back(void) {
    uint16_t next;

    if (lane == 0U) {
        return;
    }
    next = (uint16_t)(lane - 1U);
    // the camera never retreats, so a hop off the bottom of the screen is refused
    if ((uint16_t)(terrain_cam_lane() - next) > kMaxLanesBehind) {
        return;
    }
    if (terrain_blocked(next, col)) {
        return;
    }
    lane = next;
    slide_y = -kHopSlidePx;
}

static void hop_left(void) {
    if (col == 0U || terrain_blocked(lane, (uint8_t)(col - 1U))) {
        return;
    }
    --col;
    slide_x = kHopSlidePx;
}

static void hop_right(void) {
    if (col >= kMaxCol || terrain_blocked(lane, (uint8_t)(col + 1U))) {
        return;
    }
    ++col;
    slide_x = -kHopSlidePx;
}

void chick_init(void) {
    lane = 0;
    col = kChickSpawnCol;
    slide_x = 0;
    slide_y = 0;
    slide_scy = 0;
    OBP0_REG = kChickObp;
    set_sprite_data(kChickTileId, kChickFrames, kChickTiles);
    set_sprite_prop(kChickSprite, 0);
    chick_draw();
}

void chick_hover(void) {
    chick_init();
    move_sprite(kChickSprite, (uint8_t)((uint8_t)(kChickSpawnCol << 4) + kChickCellInset + kOamXOffset),
                (uint8_t)(kHoverChickScreenY + kOamYOffset));
}

void chick_update(uint8_t pressed) {
    if (!hopping()) {
        if (pressed & J_UP) {
            hop_forward();
        } else if (pressed & J_DOWN) {
            hop_back();
        } else if (pressed & J_LEFT) {
            hop_left();
        } else if (pressed & J_RIGHT) {
            hop_right();
        }
    }

    slide_x = step_toward_zero(slide_x);
    slide_y = step_toward_zero(slide_y);
    if (slide_scy != 0U) {
        slide_scy = (uint8_t)(slide_scy - kHopStepPx);
    }
    terrain_apply_scy(slide_scy);
}

void chick_draw(void) {
    uint8_t behind = (uint8_t)(terrain_cam_lane() - lane);
    uint8_t x = (uint8_t)((uint8_t)(col << 4) + kChickCellInset + (uint8_t)slide_x);
    uint8_t y = (uint8_t)(kCamLaneScreenY + (uint8_t)(behind << 4) + kChickCellInset + (uint8_t)slide_y);

    move_sprite(kChickSprite, (uint8_t)(x + kOamXOffset), (uint8_t)(y + kOamYOffset));
    set_sprite_tile(kChickSprite, hopping() ? kChickHopTileId : kChickTileId);
}

uint16_t chick_lane(void) {
    return lane;
}

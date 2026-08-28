#include "chick.h"

#include "assets.h"
#include "crossy.h"
#include "movers.h"
#include "sfx.h"
#include "terrain.h"

#include <gb/gb.h>
#include <stdint.h>

static uint16_t lane;
// 8.8 screen x of the sprite's left edge; a ride leaves it between grid columns
static uint16_t px_x;
// px still owed on the current hop; only one of the three is ever nonzero
static int8_t slide_x;
static int8_t slide_y;
static uint8_t slide_scy;
// 1 while the camera slide came from a hop; a creep's slide drags the chick down screen instead
static uint8_t slide_hop;
// 1 once a log has been boarded; clearing it is what makes the next landing snap onto one
static uint8_t riding;

static uint8_t hopping(void) {
    return (slide_x != 0 || slide_y != 0 || slide_scy != 0U) ? 1U : 0U;
}

static uint8_t left_px(void) {
    return (uint8_t)(px_x >> 8);
}

// the grid cell the chick's center sits in
static uint8_t cur_col(void) {
    uint16_t c = (uint16_t)(((uint16_t)(px_x >> 8) + kChickHalfPx) >> 4);

    return c > kMaxCol ? kMaxCol : (uint8_t)c;
}

static uint16_t col_px(uint8_t c) {
    return (uint16_t)((uint16_t)((uint8_t)((uint8_t)(c << 4) + kChickCellInset)) << 8);
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

// dry land snaps the chick back to the grid; water keeps the pixel x for a log to log hop
static void land_on(uint16_t next) {
    if (!terrain_is_water(next)) {
        px_x = col_px(cur_col());
    }
}

static void hop_forward(void) {
    uint16_t next = (uint16_t)(lane + 1U);

    if (terrain_blocked(next, cur_col())) {
        return;
    }
    land_on(next);
    lane = next;
    if (next > terrain_cam_lane()) {
        // the camera keeps pace, so the chick holds its screen row while the world slides
        terrain_advance();
        slide_scy = kHopSlidePx;
        slide_hop = 1;
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
    if (terrain_blocked(next, cur_col())) {
        return;
    }
    land_on(next);
    lane = next;
    slide_y = -kHopSlidePx;
}

static void hop_left(void) {
    uint8_t c = cur_col();

    if (c == 0U || terrain_blocked(lane, (uint8_t)(c - 1U))) {
        return;
    }
    // a ride is pixel continuous, so a side hop moves 16 px from wherever the log left the chick
    px_x = terrain_is_water(lane) ? (uint16_t)(px_x - kCellFixed) : col_px((uint8_t)(c - 1U));
    slide_x = kHopSlidePx;
}

static void hop_right(void) {
    uint8_t c = cur_col();

    if (c >= kMaxCol || terrain_blocked(lane, (uint8_t)(c + 1U))) {
        return;
    }
    px_x = terrain_is_water(lane) ? (uint16_t)(px_x + kCellFixed) : col_px((uint8_t)(c + 1U));
    slide_x = -kHopSlidePx;
}

void chick_init(void) {
    lane = 0;
    px_x = col_px(kChickSpawnCol);
    slide_x = 0;
    slide_y = 0;
    slide_scy = 0;
    slide_hop = 0;
    riding = 0;
    OBP0_REG = kChickObp;
    set_sprite_data(kChickTileId, kChickTileCount, kChickTiles);
    set_sprite_prop(kChickSprite, 0);
    chick_draw();
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
        // a blocked press never sets a slide, so only a hop that committed makes a sound
        if (hopping()) {
            sfx_hop();
        }
    }
    // the counter ticks every frame; a slide in flight defers the creep to the next settled one
    if (terrain_creep_due() && !hopping()) {
        terrain_advance();
        slide_scy = kHopSlidePx;
        slide_hop = 0;
    }

    slide_x = step_toward_zero(slide_x);
    slide_y = step_toward_zero(slide_y);
    if (slide_scy != 0U) {
        slide_scy = (uint8_t)(slide_scy - kHopStepPx);
    }
    terrain_apply_scy(slide_scy);
}

// the nearest log part to stand on, as an 8.8 offset from the log's own track position
static uint16_t board_offset(uint16_t pos) {
    int16_t d = (int16_t)((int16_t)chick_center_x() - (int16_t)(uint8_t)(pos >> 8));
    int16_t snapped = 0;

    if (d > kLogSnapPx / 2) {
        snapped = kLogSnapPx;
    } else if (d < -(kLogSnapPx / 2)) {
        snapped = -kLogSnapPx;
    }
    // the sprite's left edge, so the chick's oam x lands exactly on one log part's
    return (uint16_t)((uint16_t)(snapped - (int16_t)kChickHalfPx) << 8);
}

uint8_t chick_afloat(void) {
    int16_t step;
    uint16_t pos;

    // a hop is airborne, so the water only judges the chick once it lands
    if (hopping() || !terrain_is_water(lane)) {
        riding = 0;
        return 1;
    }
    if (!movers_log_ride(lane, chick_center_x(), &step, &pos)) {
        riding = 0;
        return 0;
    }
    if (riding == 0U) {
        px_x = (uint16_t)(pos + board_offset(pos));
        riding = 1;
    } else {
        px_x = (uint16_t)(px_x + (uint16_t)step);
    }
    // px_x wraps near 0xffff once a log drags the chick left of the screen, so one test covers both edges
    return px_x <= kRideMaxFixed ? 1U : 0U;
}

void chick_draw(void) {
    uint8_t x = (uint8_t)(left_px() + (uint8_t)slide_x);
    uint8_t y = chick_screen_y();

    move_sprite(kChickSprite, (uint8_t)(x + kOamXOffset), (uint8_t)(y + kOamYOffset));
    set_sprite_tile(kChickSprite, hopping() ? kChickHopTileId : kChickTileId);
}

void chick_hide(void) {
    // oam y 0 parks a sprite entirely above the screen
    move_sprite(kChickSprite, 0, 0);
}

uint16_t chick_lane(void) {
    return lane;
}

uint8_t chick_center_x(void) {
    return (uint8_t)((uint8_t)(left_px() + kChickHalfPx) + (uint8_t)slide_x);
}

uint8_t chick_screen_y(void) {
    uint8_t inset = terrain_is_water(lane) ? kChickWaterInset : kChickLaneInset;
    // a hop's slide is the world moving under a still chick, so its lift cancels the lane's
    uint8_t lift = slide_hop != 0U ? slide_scy : 0U;

    return (uint8_t)(terrain_lane_screen_y(lane) + inset + (uint8_t)slide_y + lift);
}

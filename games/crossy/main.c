#include "chick.h"
#include "crossy.h"
#include "eagle.h"
#include "hud.h"
#include "movers.h"
#include "save.h"
#include "sfx.h"
#include "terrain.h"

#include <gb/gb.h>
#include <gbdk/font.h>
#include <stdint.h>

enum GameState { kStateHover, kStateUnlock, kStatePlay, kStateOver };

// one band of inverted font cells serves both the hover banner and the game over popup
static uint8_t band[kPopupRows][kPopupCols];
static uint8_t band_rows;
// map row the band starts on; scy is snapped to the lane grid, so a screen row is one map row
static uint8_t band_row;
static uint8_t band_step;
static char line[16];

static uint8_t text_len(const char* text) {
    uint8_t n = 0;
    while (text[n] != '\0') {
        ++n;
    }
    return n;
}

// "LABEL n" with leading zeros trimmed
static void format_value(const char* label, uint16_t value) {
    uint8_t n = 0;
    uint8_t hundreds;
    uint8_t tens;

    if (value > kScoreMax) {
        value = kScoreMax;
    }
    while (label[n] != '\0') {
        line[n] = label[n];
        ++n;
    }
    line[n++] = ' ';
    hundreds = (uint8_t)(value / 100U);
    tens = (uint8_t)((value / 10U) % 10U);
    if (hundreds != 0U) {
        line[n++] = (char)('0' + hundreds);
    }
    if (hundreds != 0U || tens != 0U) {
        line[n++] = (char)('0' + tens);
    }
    line[n++] = (char)('0' + (uint8_t)(value % 10U));
    line[n] = '\0';
}

// gbdk's font is dark strokes on a light cell; the popup wants the reverse
static void build_inverse_font(void) {
    uint8_t glyph[kTileBytes];
    uint8_t t;
    uint8_t i;

    for (t = 0; t < kInvFontTiles; ++t) {
        get_bkg_data((uint8_t)(kFontFirstTile + t), 1, glyph);
        for (i = 0; i < kTileBytes; ++i) {
            glyph[i] = (uint8_t)~glyph[i];
        }
        set_bkg_data((uint8_t)(kInvFontFirstTile + t), 1, glyph);
    }
}

static void band_line(uint8_t row, const char* text) {
    uint8_t x = (uint8_t)((kScreenCols - text_len(text)) / 2U);
    uint8_t n = 0;

    while (text[n] != '\0') {
        band[row][x + n] = (uint8_t)(kInvFontFirstTile + (uint8_t)text[n] - kFontFirstChar);
        ++n;
    }
}

static void band_reset(uint8_t rows, uint8_t top_row) {
    uint8_t r;
    uint8_t c;

    band_rows = rows;
    for (r = 0; r < rows; ++r) {
        for (c = 0; c < kPopupCols; ++c) {
            band[r][c] = kPopupFillTileId;
        }
    }
    band_row = (uint8_t)(((uint8_t)(SCY_REG >> 3) + top_row) & (kMapRows - 1U));
    band_step = 0;
}

static void band_stage(void) {
    uint8_t i;
    for (i = 0; i < kPopupRowsPerFrame && band_step < band_rows; ++i) {
        set_bkg_tiles(0, (uint8_t)((band_row + band_step) & (kMapRows - 1U)), kPopupCols, 1, band[band_step]);
        ++band_step;
    }
}

static void build_banner(void) {
    band_reset(kBannerRows, kBannerTopRow);
    band_line(kBannerTitleRow, "CROSSY");
    band_line(kBannerPromptRow, "SPACE TO START");
    // the value itself is drawn as digit sprites, so any digit count stays pixel centered
    band_line(kBannerBestRow, "BEST");
}

// the hover world is the run's world: seeded here, generated here, previewed live
static void enter_hover(uint8_t seed) {
    // lcd off so the ring fill and the banner cannot land mid-scanline
    DISPLAY_OFF;
    BGP_REG = kTitleBgp;
    // movers first: generating a danger lane rolls that lane's traffic
    movers_init(seed);
    terrain_init(seed);
    chick_init();
    eagle_init();
    hud_init();
    hud_draw_best(save_best());
    build_banner();
    while (band_step < band_rows) {
        band_stage();
    }
    SHOW_SPRITES;
    SHOW_BKG;
    DISPLAY_ON;
}

static void build_popup(uint16_t score) {
    band_reset(kPopupRows, kPopupTopRow);
    band_line(kPopupOverRow, "GAME OVER");
    format_value("SCORE", score);
    band_line(kPopupScoreRow, line);
    format_value("BEST", save_best());
    band_line(kPopupBestRow, line);
    band_line(kPopupPromptRow, "SPACE TO RETRY");
}

static void enter_over(uint16_t score) {
    chick_hide();
    movers_hide();
    eagle_hide();
    save_record(score);
    // a mid hop death leaves scy between lanes, so snap it before the band is placed
    terrain_apply_scy(0);
    build_popup(score);
}

void main(void) {
    uint8_t state = kStateHover;
    uint8_t keys = 0;
    uint8_t prev = 0;
    uint8_t pressed = 0;
    uint8_t over_frames = 0;
    // free running; only its value at a hover entry matters, and that is the world's seed
    uint8_t frames = 0;
    uint8_t unlock_left = 0;
    uint8_t afloat = 1;
    uint8_t taken = 0;
    uint8_t drowned = 0;
    uint8_t struck = 0;
    uint16_t score = 0;
    uint16_t shown = 0;

    font_init();
    font_set(font_load(font_ibm));
    build_inverse_font();
    save_init();
    sfx_init();
    SPRITES_8x8;
    // boot samples a counter that has not run yet, so the first world is always the same one
    enter_hover(frames);

    while (1) {
        vsync();
        ++frames;
        prev = keys;
        keys = joypad();
        // edge triggered so holding a direction never autofires
        pressed = (uint8_t)(keys & (uint8_t)~prev);

        if (state == kStateOver) {
            // map writes only ever happen here, inside vblank
            band_stage();
            if (over_frames < kOverLockoutFrames) {
                ++over_frames;
            } else if (pressed != 0U) {
                // the dismiss press timing varies the seed, so a death always gives a fresh world
                enter_hover(frames);
                state = kStateHover;
            }
            continue;
        }

        if (state == kStateHover) {
            // the world runs, but nothing that could end it: no eagle, no creep, no collisions
            movers_update_to((uint16_t)(terrain_cam_lane() + kBannerLaneLo - 1U));
            // the dismissing press is spent, so only a fresh press unlocks the run
            if (pressed & (J_START | J_A)) {
                unlock_left = kBannerLanes;
                hud_draw(0);
                state = kStateUnlock;
            }
            continue;
        }

        if (state == kStateUnlock) {
            // one lane a frame: the same vblank budget the popup stages at
            --unlock_left;
            terrain_redraw_lane((uint16_t)(kBannerLaneLo + unlock_left));
            // still hidden: the banner's own lanes come back one row pair at a time
            movers_update_to((uint16_t)(terrain_cam_lane() + kBannerLaneLo - 1U));
            if (unlock_left == 0U) {
                score = 0;
                shown = 0;
                state = kStatePlay;
            }
            continue;
        }

        // the only bg writes of the run happen here, inside vblank
        if (terrain_tick_tracks()) {
            sfx_bell();
        }
        // the swoop cannot be outrun, so the buttons go dead the moment it starts
        chick_update(eagle_active() ? 0U : pressed);
        movers_update();
        // after the logs have moved, so the chick takes the very same 8.8 step they did
        afloat = chick_afloat();
        chick_draw();
        if (chick_lane() > score) {
            score = chick_lane();
            eagle_reset();
            if (score % kScoreChime == 0U) {
                sfx_score();
            }
        }
        if (score != shown) {
            shown = score;
            hud_draw(score);
        }
        // a creep that leaves the chick below the visible window calls the eagle in at once
        if ((uint16_t)(terrain_cam_lane() - chick_lane()) > kMaxLanesBehind) {
            eagle_summon();
        }
        taken = eagle_update(chick_center_x(), chick_screen_y());
        // dmg draws ten sprites a line; the swoop's two beside a train's six leave the digits no room
        if (eagle_active()) {
            hud_hide();
        }
        drowned = (uint8_t)(!eagle_active() && !afloat);
        struck = (uint8_t)(!eagle_active() && (movers_car_hit(chick_lane(), chick_center_x()) ||
                                               terrain_train_hit(chick_lane(), chick_center_x())));
        if (taken || drowned || struck) {
            if (drowned) {
                sfx_splash();
            } else {
                sfx_hit();
            }
            enter_over(score);
            over_frames = 0;
            state = kStateOver;
        }
    }
}

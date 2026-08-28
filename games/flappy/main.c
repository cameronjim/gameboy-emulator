#include "assets.h"
#include "bird.h"
#include "flappy.h"
#include "hud.h"
#include "save.h"
#include "sfx.h"
#include "world.h"

#include <gb/gb.h>
#include <gbdk/console.h>
#include <gbdk/font.h>
#include <stdint.h>
#include <stdio.h>

enum GameState { kStateTitle, kStatePlay, kStateOver };

static uint8_t strip[kScreenCols];
static uint8_t popup[kPopupRows][kPopupCols];
// map column under screen column 0 when the round froze
static uint8_t popup_col;
static uint8_t popup_step;
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

static void draw_ground_strip(void) {
    uint8_t c;
    for (c = 0; c < kScreenCols; ++c) {
        strip[c] = kGroundTileId;
    }
    set_bkg_tiles(0, kMapRows - 2U, kScreenCols, 1, strip);
    set_bkg_tiles(0, kMapRows - 1U, kScreenCols, 1, strip);
}

static void print_centered(uint8_t y, const char* text) {
    gotoxy((uint8_t)((kScreenCols - text_len(text)) / 2U), y);
    printf("%s", text);
}

// the hover screen; redrawn on every entry so a new best shows straight away
static void draw_title(void) {
    BGP_REG = kTitleBgp;
    SCX_REG = 0;
    SCY_REG = 0;
    cls();
    print_centered(kTitleTextY, "FLAPPY");
    print_centered(kPromptTextY, "SPACE TO START");
    format_value("BEST", save_best());
    print_centered(kBestTextY, line);
    set_bkg_data(kGroundTileId, 1, kGroundTile);
    draw_ground_strip();
    hud_hide();
    bird_init();
}

static void enter_title(void) {
    // lcd off so cls and the tile uploads cannot land mid-scanline
    DISPLAY_OFF;
    draw_title();
    DISPLAY_ON;
}

static void enter_play(uint8_t seed) {
    DISPLAY_OFF;
    cls();
    world_init(seed);
    bird_init();
    hud_init();
    SHOW_SPRITES;
    SHOW_BKG;
    DISPLAY_ON;
}

static void popup_line(uint8_t row, const char* text) {
    uint8_t x = (uint8_t)((kScreenCols - text_len(text)) / 2U);
    uint8_t n = 0;

    while (text[n] != '\0') {
        popup[row][x + n] = (uint8_t)(kInvFontFirstTile + (uint8_t)text[n] - kFontFirstChar);
        ++n;
    }
}

static void build_popup(uint16_t score) {
    uint8_t r;
    uint8_t c;

    for (r = 0; r < kPopupRows; ++r) {
        for (c = 0; c < kPopupCols; ++c) {
            popup[r][c] = kPopupFillTileId;
        }
    }
    popup_line(kPopupOverRow, "GAME OVER");
    format_value("SCORE", score);
    popup_line(kPopupScoreRow, line);
    format_value("BEST", save_best());
    popup_line(kPopupBestRow, line);
    popup_line(kPopupPromptRow, "SPACE TO RETRY");
    // the world is frozen, so this column stays under screen column 0 until the popup goes
    popup_col = (uint8_t)((SCX_REG >> 3) & (kMapCols - 1U));
    popup_step = 0;
}

// the bg map is a 32 column ring, so a band row can straddle column 31
static void write_band_row(uint8_t row) {
    uint8_t y = (uint8_t)(kPopupTopRow + row);
    uint8_t head = (uint8_t)(kMapCols - popup_col);

    if (head >= kPopupCols) {
        set_bkg_tiles(popup_col, y, kPopupCols, 1, popup[row]);
        return;
    }
    set_bkg_tiles(popup_col, y, head, 1, popup[row]);
    set_bkg_tiles(0, y, (uint8_t)(kPopupCols - head), 1, popup[row] + head);
}

static void stage_popup(void) {
    uint8_t i;
    for (i = 0; i < kPopupRowsPerFrame && popup_step < kPopupRows; ++i) {
        write_band_row(popup_step);
        ++popup_step;
    }
}

void main(void) {
    uint8_t state = kStateTitle;
    uint8_t keys = 0;
    uint8_t prev = 0;
    uint8_t pressed = 0;
    uint8_t over_frames = 0;
    uint8_t hover_frames = 0;
    uint8_t dead = 0;
    uint16_t shown = 0;
    uint16_t score = 0;

    font_init();
    font_set(font_load(font_ibm));
    build_inverse_font();
    sfx_init();
    save_init();
    SPRITES_8x8;
    draw_title();
    SHOW_SPRITES;
    SHOW_BKG;
    DISPLAY_ON;

    while (1) {
        vsync();
        prev = keys;
        keys = joypad();
        // edge triggered so holding a button never autofires
        pressed = (uint8_t)(keys & (uint8_t)~prev);

        if (state == kStateOver) {
            // map writes only ever happen here, inside vblank
            stage_popup();
            if (over_frames < kOverLockoutFrames) {
                ++over_frames;
            } else if (pressed != 0U) {
                enter_title();
                state = kStateTitle;
            }
            continue;
        }

        if (state == kStateTitle) {
            bird_hover();
            ++hover_frames;
            // the dismissing press is spent, so only a fresh press starts the next run
            if (pressed & (J_START | J_A)) {
                enter_play(hover_frames);
                // the press that starts the run is also its first flap
                bird_flap();
                bird_draw();
                sfx_flap();
                shown = 0;
                score = 0;
                state = kStatePlay;
            }
            continue;
        }

        if (pressed & J_A) {
            bird_flap();
            sfx_flap();
        }
        bird_update();
        bird_draw();
        world_scroll();
        dead = world_kills(bird_top_px());
        score = world_score();
        if (score != shown) {
            shown = score;
            hud_draw(score);
            sfx_score();
        }
        if (dead) {
            sfx_hit();
            bird_hide();
            save_record(score);
            world_snap_scroll();
            build_popup(score);
            over_frames = 0;
            state = kStateOver;
        }
    }
}

#include "chick.h"
#include "crossy.h"
#include "hud.h"
#include "movers.h"
#include "save.h"
#include "terrain.h"

#include <gb/gb.h>
#include <gbdk/console.h>
#include <gbdk/font.h>
#include <stdint.h>
#include <stdio.h>

enum GameState { kStateTitle, kStatePlay, kStateOver };

static uint8_t popup[kPopupRows][kPopupCols];
// map row the band starts on; the world is frozen, so it holds until the popup goes
static uint8_t popup_row;
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
    print_centered(kTitleTextY, "CROSSY");
    print_centered(kPromptTextY, "SPACE TO START");
    format_value("BEST", save_best());
    print_centered(kBestTextY, line);
    hud_hide();
    movers_hide();
    chick_hover();
}

static void enter_title(void) {
    // lcd off so cls and the redraw cannot land mid-scanline
    DISPLAY_OFF;
    draw_title();
    DISPLAY_ON;
}

static void enter_play(uint8_t seed) {
    // lcd off so cls and the ring fill cannot land mid-scanline
    DISPLAY_OFF;
    cls();
    // movers first: generating a danger lane rolls that lane's traffic
    movers_init(seed);
    terrain_init(seed);
    chick_init();
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
    // scy is snapped to the lane grid, so a screen row is exactly one map row
    popup_row = (uint8_t)(((uint8_t)(SCY_REG >> 3) + kPopupTopRow) & (kMapRows - 1U));
    popup_step = 0;
}

static void stage_popup(void) {
    uint8_t i;
    for (i = 0; i < kPopupRowsPerFrame && popup_step < kPopupRows; ++i) {
        set_bkg_tiles(0, (uint8_t)((popup_row + popup_step) & (kMapRows - 1U)), kPopupCols, 1,
                      popup[popup_step]);
        ++popup_step;
    }
}

static void enter_over(uint16_t score) {
    chick_hide();
    movers_hide();
    save_record(score);
    // a mid hop death leaves scy between lanes, so snap it before the band is placed
    terrain_apply_scy(0);
    build_popup(score);
}

void main(void) {
    uint8_t state = kStateTitle;
    uint8_t keys = 0;
    uint8_t prev = 0;
    uint8_t pressed = 0;
    uint8_t over_frames = 0;
    uint8_t hover_frames = 0;
    uint8_t afloat = 1;
    uint16_t score = 0;
    uint16_t shown = 0;

    font_init();
    font_set(font_load(font_ibm));
    build_inverse_font();
    save_init();
    SPRITES_8x8;
    hud_init();
    draw_title();
    SHOW_SPRITES;
    SHOW_BKG;
    DISPLAY_ON;

    while (1) {
        vsync();
        prev = keys;
        keys = joypad();
        // edge triggered so holding a direction never autofires
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
            ++hover_frames;
            // the dismissing press is spent, so only a fresh press starts the next run
            if (pressed & (J_START | J_A)) {
                enter_play(hover_frames);
                score = 0;
                shown = 0;
                state = kStatePlay;
            }
            continue;
        }

        // the only bg write of the run happens here, inside vblank
        chick_update(pressed);
        movers_update();
        // after the logs have moved, so the chick takes the very same 8.8 step they did
        afloat = chick_afloat();
        chick_draw();
        if (chick_lane() > score) {
            score = chick_lane();
        }
        if (score != shown) {
            shown = score;
            hud_draw(score);
        }
        if (!afloat || movers_car_hit(chick_lane(), chick_center_x())) {
            enter_over(score);
            over_frames = 0;
            state = kStateOver;
        }
    }
}

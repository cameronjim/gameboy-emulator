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

static uint8_t win_row[kWinCols];

static void draw_ground_strip(void) {
    uint8_t c;
    for (c = 0; c < kWinCols; ++c) {
        win_row[c] = kGroundTileId;
    }
    set_bkg_tiles(0, kMapRows - 2U, kWinCols, 1, win_row);
    set_bkg_tiles(0, kMapRows - 1U, kWinCols, 1, win_row);
}

static void draw_title(void) {
    BGP_REG = kTitleBgp;
    cls();
    gotoxy(kTitleTextX, kTitleTextY);
    printf("FLAPPY");
    gotoxy(kPromptTextX, kPromptTextY);
    printf("PRESS START");
    set_bkg_data(kGroundTileId, 1, kGroundTile);
    draw_ground_strip();
}

static void win_print(uint8_t x, uint8_t y, const char* text) {
    uint8_t n = 0;
    while (text[n] != '\0') {
        win_row[n] = (uint8_t)(kFontFirstTile + (uint8_t)text[n] - kFontFirstChar);
        ++n;
    }
    set_win_tiles(x, y, n, 1, win_row);
}

// one banner line: a label then the value, leading zeros trimmed, padded out to the full row
static void win_print_value(uint8_t x, uint8_t y, const char* label, uint16_t value) {
    uint8_t n = 0;
    uint8_t c;
    uint8_t hundreds;
    uint8_t tens;

    if (value > kScoreMax) {
        value = kScoreMax;
    }
    for (c = 0; c < kWinCols; ++c) {
        win_row[c] = kSkyTileId;
    }
    while (label[n] != '\0') {
        win_row[x + n] = (uint8_t)(kFontFirstTile + (uint8_t)label[n] - kFontFirstChar);
        ++n;
    }
    ++n;
    hundreds = (uint8_t)(value / 100U);
    tens = (uint8_t)((value / 10U) % 10U);
    if (hundreds != 0U) {
        win_row[x + n] = (uint8_t)(kFontFirstTile + '0' + hundreds - kFontFirstChar);
        ++n;
    }
    if (hundreds != 0U || tens != 0U) {
        win_row[x + n] = (uint8_t)(kFontFirstTile + '0' + tens - kFontFirstChar);
        ++n;
    }
    win_row[x + n] = (uint8_t)(kFontFirstTile + '0' + (uint8_t)(value % 10U) - kFontFirstChar);
    set_win_tiles(0, y, kWinCols, 1, win_row);
}

// the banner is built once with the lcd off; game over only fills in the numbers
static void build_banner(void) {
    uint8_t r;
    uint8_t c;

    for (r = 0; r < kWinRows; ++r) {
        for (c = 0; c < kWinCols; ++c) {
            win_row[c] = (r < kWinGroundRow) ? kSkyTileId : kGroundTileId;
        }
        set_win_tiles(0, r, kWinCols, 1, win_row);
    }
    win_print(kOverTextX, kOverTextY, "GAME OVER");
    win_print(kOverPromptX, kOverPromptY, "PRESS START");
    move_win(kWinX, kWinY);
}

static void enter_play(void) {
    // lcd off so cls and the tile uploads cannot land mid-scanline
    DISPLAY_OFF;
    HIDE_WIN;
    cls();
    world_init();
    build_banner();
    bird_init();
    hud_init();
    SPRITES_8x8;
    SHOW_SPRITES;
    SHOW_BKG;
    DISPLAY_ON;
}

void main(void) {
    uint8_t state = kStateTitle;
    uint8_t keys = 0;
    uint8_t prev = 0;
    uint8_t pressed = 0;
    uint8_t reveal = 0;
    uint8_t dead = 0;
    uint16_t shown = 0;
    uint16_t score = 0;

    font_init();
    font_set(font_load(font_ibm));
    sfx_init();
    save_init();
    draw_title();
    SHOW_BKG;
    DISPLAY_ON;

    while (1) {
        vsync();
        // banner numbers are written here so the window map is only touched in vblank
        if (reveal) {
            win_print_value(kScoreTextX, kScoreTextY, "SCORE", score);
            win_print_value(kBestTextX, kBestTextY, "BEST", save_best());
            SHOW_WIN;
            reveal = 0;
        }
        prev = keys;
        keys = joypad();
        // edge triggered so holding a button never autofires
        pressed = (uint8_t)(keys & (uint8_t)~prev);

        if (state != kStatePlay) {
            if (pressed & J_START) {
                enter_play();
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
            save_record(score);
            reveal = 1;
            state = kStateOver;
        }
    }
}

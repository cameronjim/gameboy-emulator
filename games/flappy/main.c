#include "bird.h"
#include "flappy.h"
#include "world.h"

#include <gb/gb.h>
#include <gbdk/console.h>
#include <gbdk/font.h>
#include <stdint.h>
#include <stdio.h>

enum GameState { kStateTitle, kStatePlay, kStateOver };

static uint8_t win_row[kWinCols];

static void draw_title(void) {
    BGP_REG = kTitleBgp;
    cls();
    gotoxy(kTitleTextX, kTitleTextY);
    printf("FLAPPY");
    gotoxy(kPromptTextX, kPromptTextY);
    printf("PRESS START");
}

static void win_print(uint8_t x, uint8_t y, const char* text) {
    uint8_t n = 0;
    while (text[n] != '\0') {
        win_row[n] = (uint8_t)(kFontFirstTile + (uint8_t)text[n] - kFontFirstChar);
        ++n;
    }
    set_win_tiles(x, y, n, 1, win_row);
}

// the banner is built once with the lcd off; game over only has to show the window
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

    font_init();
    font_set(font_load(font_ibm));
    draw_title();
    SHOW_BKG;
    DISPLAY_ON;

    while (1) {
        vsync();
        prev = keys;
        keys = joypad();
        // edge triggered so holding a button never autofires
        pressed = (uint8_t)(keys & (uint8_t)~prev);

        if (state != kStatePlay) {
            if (pressed & J_START) {
                enter_play();
                state = kStatePlay;
            }
            continue;
        }

        if (pressed & J_A) {
            bird_flap();
        }
        bird_update();
        bird_draw();
        world_scroll();
        if (world_kills(bird_top_px())) {
            SHOW_WIN;
            state = kStateOver;
        }
    }
}

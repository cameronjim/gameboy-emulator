#include "bird.h"
#include "flappy.h"

#include <gb/gb.h>
#include <gbdk/console.h>
#include <gbdk/font.h>
#include <stdint.h>
#include <stdio.h>

enum GameState { kStateTitle, kStatePlay };

static void draw_title(void) {
    BGP_REG = kTitleBgp;
    cls();
    gotoxy(kTitleTextX, kTitleTextY);
    printf("FLAPPY");
    gotoxy(kPromptTextX, kPromptTextY);
    printf("PRESS START");
}

static void enter_play(void) {
    // lcd off so cls and the tile upload cannot land mid-scanline
    DISPLAY_OFF;
    cls();
    bird_init();
    SPRITES_8x8;
    SHOW_SPRITES;
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

        if (state == kStateTitle) {
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
    }
}

#include "chick.h"
#include "crossy.h"
#include "hud.h"
#include "terrain.h"

#include <gb/gb.h>
#include <gbdk/console.h>
#include <gbdk/font.h>
#include <stdint.h>
#include <stdio.h>

enum GameState { kStateTitle, kStatePlay };

static uint8_t text_len(const char* text) {
    uint8_t n = 0;
    while (text[n] != '\0') {
        ++n;
    }
    return n;
}

static void print_centered(uint8_t y, const char* text) {
    gotoxy((uint8_t)((kScreenCols - text_len(text)) / 2U), y);
    printf("%s", text);
}

// the hover screen; the chick stands on its spawn cell while the title shows
static void draw_title(void) {
    BGP_REG = kTitleBgp;
    SCX_REG = 0;
    SCY_REG = 0;
    cls();
    print_centered(kTitleTextY, "CROSSY");
    print_centered(kPromptTextY, "SPACE TO START");
    print_centered(kBestTextY, "BEST 0");
    hud_hide();
    chick_hover();
}

static void enter_play(uint8_t seed) {
    // lcd off so cls and the ring fill cannot land mid-scanline
    DISPLAY_OFF;
    cls();
    terrain_init(seed);
    chick_init();
    hud_init();
    SHOW_SPRITES;
    SHOW_BKG;
    DISPLAY_ON;
}

void main(void) {
    uint8_t state = kStateTitle;
    uint8_t keys = 0;
    uint8_t prev = 0;
    uint8_t pressed = 0;
    uint8_t hover_frames = 0;
    uint16_t score = 0;
    uint16_t shown = 0;

    font_init();
    font_set(font_load(font_ibm));
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

        if (state == kStateTitle) {
            ++hover_frames;
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
        chick_draw();
        if (chick_lane() > score) {
            score = chick_lane();
        }
        if (score != shown) {
            shown = score;
            hud_draw(score);
        }
    }
}

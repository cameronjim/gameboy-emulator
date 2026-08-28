#include "hud.h"

#include "assets.h"
#include "crossy.h"

#include <gb/gb.h>
#include <stdint.h>

void hud_init(void) {
    uint8_t i;

    set_sprite_data(kDigitTileId, kDigitTileCount, kDigitTiles);
    for (i = 0; i < kHudDigits; ++i) {
        set_sprite_prop((uint8_t)(kHudFirstSprite + i), 0);
    }
    hud_draw(0);
}

void hud_hide(void) {
    uint8_t i;
    // oam y 0 parks a sprite entirely above the screen
    for (i = 0; i < kHudDigits; ++i) {
        move_sprite((uint8_t)(kHudFirstSprite + i), 0, 0);
    }
}

// the run of digit sprites, pixel centered on center_x whatever its length
static void draw_digits(uint16_t value, uint8_t center_x, uint8_t oam_y) {
    uint8_t digit[kHudDigits];
    uint8_t shown;
    uint8_t i;
    uint8_t s;
    uint8_t x;

    if (value > kScoreMax) {
        value = kScoreMax;
    }
    digit[0] = (uint8_t)(value / 100U);
    digit[1] = (uint8_t)((value / 10U) % 10U);
    digit[2] = (uint8_t)(value % 10U);
    shown = digit[0] != 0U ? 3U : (digit[1] != 0U ? 2U : 1U);

    x = (uint8_t)(center_x - (uint8_t)(shown * kDigitHalfPx));
    for (i = 0; i < kHudDigits; ++i) {
        s = (uint8_t)(kHudFirstSprite + i);
        if (i < shown) {
            // the glyph badge is the pair's top tile, so a digit is two tiles on from the last
            set_sprite_tile(
                s, (uint8_t)(kDigitTileId + (uint8_t)(digit[kHudDigits - shown + i] * kTilesPerSprite)));
            move_sprite(s, (uint8_t)(x + (uint8_t)(i * kDigitWidthPx)), oam_y);
        } else {
            // oam y 0 parks a sprite entirely above the screen
            move_sprite(s, 0, 0);
        }
    }
}

void hud_draw(uint16_t score) {
    draw_digits(score, kHudCenterOamX, kHudOamY);
}

void hud_draw_best(uint16_t best) {
    draw_digits(best, kHoverBestCenterOamX, kHoverBestOamY);
}

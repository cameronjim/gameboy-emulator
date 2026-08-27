#include "hud.h"

#include "assets.h"
#include "flappy.h"

#include <gb/gb.h>
#include <stdint.h>

void hud_init(void) {
    uint8_t i;

    set_sprite_data(kDigitTileId, kDigitCount, kDigitTiles);
    // digits must never inherit the bg priority the dead bird leaves on its own sprite
    for (i = 0; i < kHudDigits; ++i) {
        set_sprite_prop((uint8_t)(kHudFirstSprite + i), 0);
    }
    hud_draw(0);
}

void hud_draw(uint16_t score) {
    uint8_t digit[kHudDigits];
    uint8_t shown;
    uint8_t i;
    uint8_t s;
    uint8_t x;

    if (score > kScoreMax) {
        score = kScoreMax;
    }
    digit[0] = (uint8_t)(score / 100U);
    digit[1] = (uint8_t)((score / 10U) % 10U);
    digit[2] = (uint8_t)(score % 10U);
    shown = digit[0] != 0U ? 3U : (digit[1] != 0U ? 2U : 1U);

    x = (uint8_t)(kHudCenterOamX - (uint8_t)(shown * (kDigitWidthPx / 2U)));
    for (i = 0; i < kHudDigits; ++i) {
        s = (uint8_t)(kHudFirstSprite + i);
        if (i < shown) {
            set_sprite_tile(s, (uint8_t)(kDigitTileId + digit[kHudDigits - shown + i]));
            move_sprite(s, (uint8_t)(x + (uint8_t)(i * kDigitWidthPx)), kHudOamY);
        } else {
            // oam y 0 parks a sprite entirely above the screen
            move_sprite(s, 0, 0);
        }
    }
}

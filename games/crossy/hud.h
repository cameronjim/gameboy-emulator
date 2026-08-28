#ifndef HUD_H
#define HUD_H

#include <stdint.h>

void hud_init(void);
void hud_draw(uint16_t score);
// the same three sprites down on the hover banner's own row
void hud_draw_best(uint16_t best);
// parks every digit offscreen for the eagle's swoop
void hud_hide(void);

#endif

#ifndef BIRD_H
#define BIRD_H

#include <stdint.h>

void bird_init(void);
void bird_flap(void);
void bird_update(void);
// title only: bobs and cycles the wing frames with no gravity
void bird_hover(void);
// parks the bird offscreen once the round ends
void bird_hide(void);
void bird_draw(void);
uint8_t bird_top_px(void);

#endif

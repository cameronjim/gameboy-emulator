#ifndef BIRD_H
#define BIRD_H

#include <stdint.h>

void bird_init(void);
void bird_flap(void);
void bird_update(void);
void bird_draw(void);
uint8_t bird_top_px(void);

#endif

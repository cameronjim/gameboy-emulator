#ifndef CHICK_H
#define CHICK_H

#include <stdint.h>

// uploads the sprite frames and parks the chick on its spawn cell
void chick_init(void);

// title only: the chick stands a cell below its spawn row, clear of the title text
void chick_hover(void);

// starts a hop from the frame's fresh presses, then steps the slide 2 px
void chick_update(uint8_t pressed);

void chick_draw(void);

uint16_t chick_lane(void);

#endif

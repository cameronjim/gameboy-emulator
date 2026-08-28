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

void chick_hide(void);

// the lane the chick answers to; a hop commits it the frame it starts
uint16_t chick_lane(void);

// screen x of the chick sprite's center, slide included
uint8_t chick_center_x(void);

#endif

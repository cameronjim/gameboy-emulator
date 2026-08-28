#ifndef CHICK_H
#define CHICK_H

#include <stdint.h>

// uploads the sprite frames and parks the chick on its spawn cell
void chick_init(void);

// starts a hop from the frame's fresh presses, then steps the slide 2 px
void chick_update(uint8_t pressed);

// drifts the chick along with the log under it; 0 once the water has drowned or carried it off
uint8_t chick_afloat(void);

void chick_draw(void);

void chick_hide(void);

// the lane the chick answers to; a hop commits it the frame it starts
uint16_t chick_lane(void);

// screen x of the chick sprite's center, slide included
uint8_t chick_center_x(void);

// screen y of the chick sprite's top row, slide included
uint8_t chick_screen_y(void);

#endif

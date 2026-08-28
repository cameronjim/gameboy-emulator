#ifndef EAGLE_H
#define EAGLE_H

#include <stdint.h>

// uploads the wing tiles, parks the pair and clears the idle timer
void eagle_init(void);

// the chick reached a new furthest lane, so the idle timer starts over
void eagle_reset(void);

// the swoop starts on the next update whatever the timer reads; a swoop already on is left alone
void eagle_summon(void);

// 1 while the swoop is on, when the run is lost and the buttons are dead
uint8_t eagle_active(void);

// counts the idle frames and drives the dive; 1 the frame the chick is taken
uint8_t eagle_update(uint8_t chick_x, uint8_t chick_y);

void eagle_hide(void);

#endif

#ifndef CARS_H
#define CARS_H

#include <stdint.h>

// uploads the car tiles and parks the sprite pool; must run before terrain generates a lane
void cars_init(uint8_t seed);

// rolls one road lane's direction, speed and starting phase into its ring slot
void cars_lane_init(uint8_t slot);

// advances and draws only the road lanes the camera can see
void cars_update(void);

void cars_hide(void);

// 1 when a car of that lane overlaps a chick centered on center_x
uint8_t cars_hit(uint16_t lane, uint8_t center_x);

#endif

#ifndef MOVERS_H
#define MOVERS_H

#include <stdint.h>

// uploads the car and log tiles and parks the shared pool; must run before terrain generates a lane
void movers_init(uint8_t seed);

// rolls one danger lane's direction, speed and starting phase into its ring slot
void movers_lane_init(uint8_t slot, uint8_t water);

// advances and draws only the danger lanes the camera can see
void movers_update(void);

void movers_hide(void);

// 1 when a car of that road lane overlaps a chick centered on center_x
uint8_t movers_car_hit(uint16_t lane, uint8_t center_x);

// 1 when a log of that water lane carries a chick centered on center_x; step takes its 8.8 drift
uint8_t movers_log_ride(uint16_t lane, uint8_t center_x, int16_t* step);

#endif

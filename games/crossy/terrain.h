#ifndef TERRAIN_H
#define TERRAIN_H

#include <stdint.h>

// fills the 32 row ring with grass, generates the first visible lanes, parks the camera on lane 0
void terrain_init(uint8_t seed);

// 1 when a tree stands on that cell; only lanes still held by the ring may be asked
uint8_t terrain_blocked(uint16_t lane, uint8_t col);

uint16_t terrain_cam_lane(void);

// moves the camera one lane forward and streams the single lane that just came into range
void terrain_advance(void);

// scy for the current camera lane, offset by the hop's remaining slide in px
void terrain_apply_scy(uint8_t slide_px);

#endif
